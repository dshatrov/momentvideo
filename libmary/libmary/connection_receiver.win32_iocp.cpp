/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>
#include <libmary/util_dev.h>

#include <libmary/connection_receiver.h>


namespace M {

static LogGroup libMary_logGroup_receiver ("connection_receiver", LogLevel::I);

ConnectionReceiver::ReceiverOverlapped::~ReceiverOverlapped ()
{
    delete[] recv_buf;
}

mt_sync_domain (inputIoComplete) void
ConnectionReceiver::initiateReceive ()
{
    logD (receiver, _this_func_);

    if (overlapped_pending) {
        logD (receiver, _this_func, "overlapped_pending");
        return;
    }

    assert (recv_buf_pos < recv_buf_len); /* toread > 0 */
    toread = recv_buf_len - recv_buf_pos;
    logD (receiver, _this_func, "toread: ", toread);

    receiver_overlapped->ref ();

    Size nread = 0;
    AsyncIoResult const res = conn->read (receiver_overlapped,
                                          Memory (receiver_overlapped->recv_buf + recv_buf_pos, toread),
                                          &nread);
    if (res == AsyncIoResult::Error) {
        logE (receiver, _this_func, "recv failed: ", exc->toString());

        if (frontend) {
            assert (recv_accepted_pos <= recv_buf_pos);
            frontend.call (frontend->processError,
                           /*(*/
                               exc,
                               Memory (receiver_overlapped->recv_buf + recv_accepted_pos,
                                       recv_buf_pos - recv_accepted_pos)
                           /*)*/);
        }

        receiver_overlapped->unref ();
        return;
    }

    overlapped_pending = true;

    logD (receiver, _this_func, "res: ", res, ", nread: ", nread);
}

mt_sync_domain (inputIoComplete) void
ConnectionReceiver::doProcessInput ()
{
    Size const toprocess = recv_buf_pos - recv_accepted_pos;

    logD (receiver, _this_func,
          "recv_accepted_pos: ", recv_accepted_pos, ", "
          "recv_buf_pos: ", recv_buf_pos, ", "
          "toprocess: ", toprocess);

    Size num_accepted;
    ProcessInputResult res;
    if (frontend) {
        if (!frontend.call_ret<ProcessInputResult> (
                    &res,
                    frontend->processInput,
                    /*(*/
                        Memory (receiver_overlapped->recv_buf + recv_accepted_pos, toprocess),
                        &num_accepted
                    /*)*/))
        {
            res = ProcessInputResult::Error;
            num_accepted = 0;
        }
    } else {
        res = ProcessInputResult::Normal;
        num_accepted = toprocess;
    }
    assert (num_accepted <= toprocess);

    switch (res.val()) {
        case ProcessInputResult::Normal:
            assert (num_accepted == toprocess);
            recv_buf_pos = 0;
            recv_accepted_pos = 0;

            initiateReceive ();
            break;
        case ProcessInputResult::Error:
            if (frontend && frontend->processError) {
                InternalException internal_exc (InternalException::FrontendError);
                assert (recv_accepted_pos <= recv_buf_pos);
                frontend.call (frontend->processError,
                               /*(*/
                                   &internal_exc,
                                   Memory (receiver_overlapped->recv_buf + recv_accepted_pos,
                                           recv_buf_pos - recv_accepted_pos)
                               /*)*/);
            }
            return;
        case ProcessInputResult::Again:
            recv_accepted_pos += num_accepted;
            if (recv_accepted_pos > 0) {
                if (recv_buf_len - recv_accepted_pos <= (recv_buf_len >> 1)) {
                    {
                        Size const tocopy = toprocess - num_accepted;
                        if (recv_accepted_pos >= tocopy) {
                            memcpy (receiver_overlapped->recv_buf,
                                    receiver_overlapped->recv_buf + recv_accepted_pos,
                                    tocopy);
                        } else {
                            memmove (receiver_overlapped->recv_buf,
                                     receiver_overlapped->recv_buf + recv_accepted_pos,
                                     tocopy);
                        }
                    }
                    recv_buf_pos = toprocess - num_accepted;
                    recv_accepted_pos = 0;
                }
            }
            // If the buffer is full and the frontend wants more data, then
            // we fail to serve the client. This should never happen with
            // properly written frontends.
            if (recv_buf_pos >= recv_buf_len) {
                logF (receiver, _this_func, "Read buffer is full, frontend should have consumed some data. "
                      "recv_accepted_pos: ", recv_accepted_pos, ", "
                      "recv_buf_pos: ", recv_buf_pos, ", "
                      "recv_buf_len: ", recv_buf_len);
                unreachable ();
            }

            initiateReceive ();
            break;
        case ProcessInputResult::InputBlocked:
            recv_accepted_pos += num_accepted;
            return;
        default:
            unreachable ();
    }
}

mt_sync_domain (inputIoComplete) void
ConnectionReceiver::doStart ()
{
    initiateReceive ();
}

mt_sync_domain (inputIoComplete) bool
ConnectionReceiver::startTask (void * const _self)
{
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);
    self->doStart ();
    return false /* do notreschedule */;
}

mt_sync_domain (inputIoComplete) bool
ConnectionReceiver::unblockInputTask (void * const _self)
{
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);
    self->doProcessInput ();
    return false /* do notreschedule */;
}

mt_sync_domain (inputIoComplete) void
ConnectionReceiver::inputIoComplete (Exception  * const exc_,
                                     Overlapped * const mt_nonnull overlapped,
                                     Size         const nread,
                                     void       * const _self)
{
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);
    assert (overlapped == self->receiver_overlapped);

    logD (receiver, _self_func, "nread: ", nread);

    assert (self->overlapped_pending);
    self->overlapped_pending = false;

    assert (nread <= self->toread);

    if (exc_) {
        logD (receiver, _self_func, "exception: ", exc_->toString());
        if (self->frontend) {
            assert (self->recv_accepted_pos <= self->recv_buf_pos);
            self->frontend.call (self->frontend->processError,
                                 /*(*/
                                     exc_,
                                     Memory (self->receiver_overlapped->recv_buf + self->recv_accepted_pos,
                                             self->recv_buf_pos - self->recv_accepted_pos)
                                 /*)*/);
        }
        return;
    }

    if (nread == 0) {
        logD (receiver, _self_func, "EOF");
        if (self->frontend && self->frontend->processEof) {
            assert (self->recv_accepted_pos <= self->recv_buf_pos);
            self->frontend.call (self->frontend->processEof,
                                 /*(*/
                                     Memory (self->receiver_overlapped->recv_buf + self->recv_accepted_pos,
                                             self->recv_buf_pos - self->recv_accepted_pos)
                                 /*)*/);
        }
        return;
    }

    self->recv_buf_pos += nread;
    assert (self->recv_accepted_pos <= self->recv_buf_pos);
    self->doProcessInput ();
}

void
ConnectionReceiver::unblockInput ()
{
    logD (receiver, _this_func_);
    assert (deferred_reg.isValid());
    deferred_reg.scheduleTask (&unblock_input_task, false /* permanent */);
}

void
ConnectionReceiver::start ()
{
    logD (receiver, _this_func_);
    if (deferred_reg.isValid()) {
        deferred_reg.scheduleTask (&start_task, false /* permanent */);
    } else {
        doStart ();
    }
}

mt_const void
ConnectionReceiver::init (AsyncInputStream  * const mt_nonnull conn,
                          DeferredProcessor * const deferred_processor,
                          bool                const /* block_input */)
{
    this->conn = conn;

    if (deferred_processor)
        deferred_reg.setDeferredProcessor (deferred_processor);

    receiver_overlapped = grab (new (std::nothrow) ReceiverOverlapped);
    receiver_overlapped->io_complete_cb =
            CbDesc<Overlapped::IoCompleteCallback> (inputIoComplete, this, this);
    receiver_overlapped->recv_buf = new (std::nothrow) Byte [recv_buf_len];
    assert (receiver_overlapped->recv_buf);
}

ConnectionReceiver::ConnectionReceiver (EmbedContainer * const embed_container)
    : Receiver (embed_container),
      // TODO Config parameter for ConnectionReceiver input buffer size
      recv_buf_len (1 << 16 /* 64 Kb */),
      recv_buf_pos (0),
      recv_accepted_pos (0),
      toread (0),
      overlapped_pending (false)
{
    start_task.cb         = CbDesc<DeferredProcessor::TaskCallback> (startTask,        this, this);
    unblock_input_task.cb = CbDesc<DeferredProcessor::TaskCallback> (unblockInputTask, this, this);
}

ConnectionReceiver::~ConnectionReceiver ()
{
    deferred_reg.release ();
}

}

