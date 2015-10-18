/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>
#include <libmary/util_dev.h>

#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/connection_receiver.generic.h>
#else
  #include <libmary/connection_receiver.h>
#endif


namespace M {

static LogGroup libMary_logGroup_msg ("msg", LogLevel::N);

mt_sync_domain (conn_input_frontend) void
ConnectionReceiver::doProcessInput ()
{
    logD (msg, _func_);

    if (block_input || error_reported)
        return;

    for (;;) {
	assert (recv_buf_pos <= recv_buf_len);
	Size const toread = recv_buf_len - recv_buf_pos;

	Size nread = 0;
	AsyncIoResult io_res = AsyncIoResult::Normal;
	if (toread) {
            if (!use_recvfrom) {
                io_res = conn->read (Memory (recv_buf + recv_buf_pos, toread),
                                     &nread);
            } else {
                io_res = conn->recvFrom (Memory (recv_buf + recv_buf_pos, toread),
                                         &nread,
                                         &from_addr);
            }
	    logD (msg, _func, "read(): ", io_res);
	    switch ((AsyncIoResult::Value) io_res) {
		case AsyncIoResult::Again: {
		    return;
		} break;
		case AsyncIoResult::Error: {
		    logD_ (_func, "read() failed: ", exc->toString());
		    if (!error_reported) {
			error_reported = true;
			if (frontend && frontend->processError) {
                            assert (recv_accepted_pos <= recv_buf_pos);
			    frontend.call (frontend->processError,
                                           /*(*/
                                               exc,
                                               Memory (recv_buf + recv_accepted_pos,
                                                       recv_buf_pos - recv_accepted_pos)
                                           /*)*/);
                        }
		    }
		    return;
		} break;
		case AsyncIoResult::Eof: {
		    if (frontend && frontend->processEof) {
                        assert (recv_accepted_pos <= recv_buf_pos);
			frontend.call (frontend->processEof,
                                       /*(*/
                                           Memory (recv_buf + recv_accepted_pos,
                                                   recv_buf_pos - recv_accepted_pos)
                                       /*)*/);
                    }
		    return;
		} break;
		case AsyncIoResult::Normal:
		case AsyncIoResult::Normal_Again:
		case AsyncIoResult::Normal_Eof:
		  // No-op
		    break;
		default:
		    unreachable ();
	    }
	}
	assert (nread <= toread);
	recv_buf_pos += nread;

	logD (msg, _func, "nread: ", nread, ", recv_accepted_pos: ", recv_accepted_pos, ", recv_buf_pos: ", recv_buf_pos);

	assert (recv_accepted_pos <= recv_buf_pos);
	Size const toprocess = recv_buf_pos - recv_accepted_pos;
	Size num_accepted;
	ProcessInputResult res;
        if (frontend) {
            if (!use_recvfrom) {
                if (!frontend.call_ret<ProcessInputResult> (
                            &res,
                            frontend->processInput,
                            /*(*/
                                Memory (recv_buf + recv_accepted_pos, toprocess),
                                &num_accepted
                            /*)*/))
                {
                    res = ProcessInputResult::Error;
                    num_accepted = 0;
                }
            } else {
                if (!frontend.call_ret<ProcessInputResult> (
                            &res,
                            frontend->processInputFrom,
                            /*(*/
                                Memory (recv_buf + recv_accepted_pos, toprocess),
                                from_addr,
                                &num_accepted
                            /*)*/))
                {
                    res = ProcessInputResult::Error;
                    num_accepted = 0;
                }
            }
        } else {
            res = ProcessInputResult::Normal;
            num_accepted = toprocess;
        }
	assert (num_accepted <= toprocess);
	logD (msg, _func, res);
	switch (res.val()) {
	    case ProcessInputResult::Normal:
		assert (num_accepted == toprocess);
		recv_buf_pos = 0;
		recv_accepted_pos = 0;
		break;
	    case ProcessInputResult::Error:
		if (!error_reported) {
		    error_reported = true;
		    if (frontend && frontend->processError) {
			InternalException internal_exc (InternalException::FrontendError);
                        assert (recv_accepted_pos <= recv_buf_pos);
                        frontend.call (frontend->processError,
                                       /*(*/
                                           &internal_exc,
                                           Memory (recv_buf + recv_accepted_pos,
                                                   recv_buf_pos - recv_accepted_pos)
                                       /*)*/);
		    }
		}
		return;
	    case ProcessInputResult::Again:
		recv_accepted_pos += num_accepted;
		if (recv_accepted_pos > 0) {
                    // This also ensures that memcpy() is enough and memmove() is not needed.
		    if (recv_buf_len - recv_accepted_pos <= (recv_buf_len >> 1)) {
			logD (msg, _func, "shifting receive buffer, toprocess: ", toprocess, ", num_accepted: ", num_accepted);
                        Size const tocopy = toprocess - num_accepted;

                        assert (recv_accepted_pos >= tocopy);
                        memcpy (recv_buf, recv_buf + recv_accepted_pos, tocopy);

			recv_buf_pos = tocopy;
			recv_accepted_pos = 0;
		    }
		}
		// If the buffer is full and the frontend wants more data, then
		// we fail to serve the client. This should never happen with
		// properly written frontends.
		if (recv_buf_pos >= recv_buf_len) {
                    logLock ();
		    logW_locked_ (_this_func, "Read buffer is full, frontend should have consumed some data. "
                                  "recv_accepted_pos: ", recv_accepted_pos, ", "
                                  "recv_buf_pos: ", recv_buf_pos, ", "
                                  "recv_buf_len: ", recv_buf_len);
                    // hexdump (logs, ConstMemory (recv_buf, recv_buf_len));
                    logUnlock ();

                    InternalException tmp_exc (InternalException::BadInput);
                    processError (&tmp_exc, this);
                    return;
		}
		break;
            case ProcessInputResult::InputBlocked:
                recv_accepted_pos += num_accepted;
                return;
	    default:
		unreachable ();
	}

	if (io_res == AsyncIoResult::Normal_Again)
	    return;

	if (io_res == AsyncIoResult::Normal_Eof) {
	    if (frontend && frontend->processEof) {
                assert (recv_accepted_pos <= recv_buf_pos);
                frontend.call (frontend->processEof,
                               /*(*/
                                   Memory (recv_buf + recv_accepted_pos,
                                           recv_buf_pos - recv_accepted_pos)
                               /*)*/);
            }
	    return;
	}
    } // for (;;)
}

AsyncInputStream::InputFrontend const ConnectionReceiver::conn_input_frontend = {
    processInput,
    processError
};

mt_sync_domain (conn_input_frontend) void
ConnectionReceiver::processInput (void * const _self)
{
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);
    self->doProcessInput ();
}

mt_sync_domain (conn_input_frontend) void
ConnectionReceiver::processError (Exception * const exc_,
				  void      * const _self)
{
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);

    self->error_received = true;
    if (self->block_input || self->error_reported) {
	return;
    }
    self->error_reported = true;

    if (self->frontend && self->frontend->processError) {
        assert (self->recv_accepted_pos <= self->recv_buf_pos);
        self->frontend.call (self->frontend->processError,
                             /*(*/
                                 exc_,
                                 Memory (self->recv_buf + self->recv_accepted_pos,
                                         self->recv_buf_pos - self->recv_accepted_pos)
                             /*)*/);
    }
}

mt_sync_domain (conn_input_frontend) void
ConnectionReceiver::doUnblockInput ()
{
    if (error_received && !error_reported) {
        error_reported = true;
        // There's little value in saving original exception from processError().
        // That would require extra synchronization.
        InternalException exc_ (InternalException::UnknownError);
        if (frontend && frontend->processError) {
            assert (recv_accepted_pos <= recv_buf_pos);
            frontend.call (frontend->processError,
                           /*(*/
                               &exc_,
                               Memory (recv_buf + recv_accepted_pos,
                                       recv_buf_pos - recv_accepted_pos)
                           /*)*/);
        }
    }

    block_input = false;
    doProcessInput ();
}

mt_sync_domain (conn_input_frontend) bool
ConnectionReceiver::unblockInputTask (void * const _self)
{
    ConnectionReceiver * const self = static_cast <ConnectionReceiver*> (_self);
    self->doUnblockInput ();
    return false;
}

void
ConnectionReceiver::unblockInput ()
{
    assert (deferred_reg.isValid());

    deferred_reg.scheduleTask (&unblock_input_task, false /* permanent */);
}

void
ConnectionReceiver::start ()
{
    if (deferred_reg.isValid()) {
        // TODO For FreeBSD+kqueue, always schedule unblock_input_task?
        if (block_input)
            deferred_reg.scheduleTask (&unblock_input_task, false /* permanent */);
    } else {
        doUnblockInput ();
    }
}

mt_const void
ConnectionReceiver::init (AsyncInputStream  * const mt_nonnull conn,
                          DeferredProcessor * const deferred_processor,
                          bool                const block_input,
                          bool                const use_recvfrom)
{
    this->conn = conn;
    this->use_recvfrom = use_recvfrom;
    this->block_input = block_input;

    if (deferred_processor)
        deferred_reg.setDeferredProcessor (deferred_processor);

    recv_buf = new (std::nothrow) Byte [recv_buf_len];
    assert (recv_buf);

    conn->setInputFrontend (CbDesc<AsyncInputStream::InputFrontend> (&conn_input_frontend, this, this));
}

ConnectionReceiver::ConnectionReceiver (EmbedContainer * const embed_container)
    : Receiver (embed_container),
      use_recvfrom      (false),
      recv_buf          (NULL),
      recv_buf_len      (1 << 16 /* 64 Kb */),
      recv_buf_pos      (0),
      recv_accepted_pos (0),
      block_input       (false),
      error_received    (false),
      error_reported    (false)
{
    unblock_input_task.cb = CbDesc<DeferredProcessor::TaskCallback> (unblockInputTask, this, this);
}

ConnectionReceiver::~ConnectionReceiver ()
{
    delete[] recv_buf;
}

}

