/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>

#include <libmary/immediate_connection_sender.h>


namespace M {

// Must be called with 'mutex' held. Releases 'mutex' before returning.
mt_unlocks (mutex) void
ImmediateConnectionSender::closeIfNeeded (bool const deferred_event,
                                          bool const unlock)
{
    if (!closed
        && close_after_flush
        && !conn_sender_impl.gotDataToSend ())
    {
        closed = true;
        if (unlock) {
            mutex.unlock ();
        } else {
            assert (deferred_event);
        }

        if (deferred_event) {
            fireClosed_deferred (&deferred_reg, NULL /* exc_buf */);
            if (frontend) {
                frontend.call_deferred (&deferred_reg,
                                        frontend->closed,
                                        NULL /* extra_ref_data */,
                                        static_cast <Exception*> (NULL) /* exc_ */);
            }
        } else {
            fireClosed (NULL /* exc_ */);
            if (frontend)
                frontend.call (frontend->closed, /*(*/ (Exception*) NULL /* exc_ */ /*)*/);
        }
    } else {
        if (unlock)
            mutex.unlock ();
    }
}

#ifdef LIBMARY_WIN32_IOCP
void
ImmediateConnectionSender::outputIoComplete (Exception  * const exc_,
                                             Overlapped * const /* overlapped */,
                                             Size         const /* bytes_transferred */,
                                             void       * const _self)
{
#else
AsyncOutputStream::OutputFrontend const ImmediateConnectionSender::conn_output_frontend = {
    processOutput
};

void
ImmediateConnectionSender::processOutput (void * const _self)
{
#endif
    ImmediateConnectionSender * const self = static_cast <ImmediateConnectionSender*> (_self);

    self->mutex.lock ();

  #ifdef LIBMARY_WIN32_IOCP
    self->conn_sender_impl.outputComplete ();

    if (exc_) {
        self->mutex.unlock ();
        if (self->frontend)
            self->frontend.call (self->frontend->closed, /*(*/ exc_ /*)*/);

        return;
    }
  #endif

    AsyncIoResult const res = self->conn_sender_impl.sendPendingMessages ();
    if (res == AsyncIoResult::Error ||
        res == AsyncIoResult::Eof)
    {
        self->ready_for_output = false;

        bool inform_closed = false;
        if (!self->closed) {
            self->closed = true;
            inform_closed = true;
        }

        self->mutex.unlock ();

        // exc is NULL for Eof.
        if (res == AsyncIoResult::Error) {
            logE_ (_func, exc->toString());

            if (inform_closed) {
                ExceptionBuffer * const exc_buf = exc_swap_nounref ();

                self->fireClosed (exc_buf->getException());
                if (self->frontend) {
                    self->frontend.call (self->frontend->closed,
                                         /*(*/ exc_buf->getException() /*)*/);
                }

                exc_delete (exc_buf);
            }
        } else {
            if (inform_closed) {
                self->fireClosed (NULL /* exc_ */);
                if (self->frontend) {
                    self->frontend.call (self->frontend->closed,
                                         /*(*/ static_cast <Exception*> (NULL) /* exc_ */ /*)*/);
                }
            }
        }

        return;
    }

    if (res == AsyncIoResult::Again)
        self->ready_for_output = false;
    else
        self->ready_for_output = true;

    mt_unlocks (mutex) self->closeIfNeeded (false /* deferred_event */, true /* unlock */);
}

void
ImmediateConnectionSender::sendMessage (SenderMessageEntry * const mt_nonnull msg_entry,
                                        bool                 const do_flush)
{
    mutex.lock ();
    conn_sender_impl.queueMessage (msg_entry);
    if (do_flush) {
        mt_unlocks (mutex) doFlush (true /* unlock */);
        return;
    }
    mutex.unlock ();
}

void
ImmediateConnectionSender::sendMessage (SenderMessageEntry  * const mt_nonnull msg_entry,
                                        bool                  const do_flush,
                                        SenderStateCallback * const sender_state_cb,
                                        void                * const sender_state_cb_data)
{
    mutex.lock ();
    conn_sender_impl.queueMessage (msg_entry);

    if (sender_state_cb)
        sender_state_cb (conn_sender_impl.getSenderState(), sender_state_cb_data);

    if (do_flush) {
        mt_unlocks (mutex) doFlush (true /* unlock */);
        return;
    }
    mutex.unlock ();
}

mt_mutex (mutex) void
ImmediateConnectionSender::sendMessage_locked (SenderMessageEntry * const mt_nonnull msg_entry,
                                               bool                 const do_flush)
{
    conn_sender_impl.queueMessage (msg_entry);
    if (do_flush)
        flush_locked ();
}

mt_mutex (mutex) mt_unlocks (mutex) void
ImmediateConnectionSender::doFlush (bool const unlock)
{
    if (!ready_for_output) {
        if (unlock) {
            mutex.unlock ();
        }
        return;
    }

    AsyncIoResult const res = conn_sender_impl.sendPendingMessages ();
    if (res == AsyncIoResult::Error ||
        res == AsyncIoResult::Eof)
    {
        ready_for_output = false;

        bool inform_closed = false;
        if (!closed) {
            closed = true;
            inform_closed = true;
        }

        if (unlock)
            mutex.unlock ();

        // TODO It might be better to return Result from flush().
        //
        // exc is NULL for Eof.
        if (res == AsyncIoResult::Error) {
            logE_ (_func, exc->toString());

            if (inform_closed) {
                Ref<ExceptionBuffer> const exc_buf = exc_swap ();

                fireClosed_deferred (&deferred_reg, exc_buf);
                if (frontend) {
                    frontend.call_deferred (&deferred_reg,
                                            frontend->closed,
                                            exc_buf /* extra_ref_data */,
                                            exc_buf->getException());
                }
            }
        } else {
            if (inform_closed) {
                fireClosed_deferred (&deferred_reg, NULL /* exc_buf */);
                if (frontend) {
                    frontend.call_deferred (&deferred_reg,
                                            frontend->closed,
                                            NULL /* extra_ref_data */,
                                            static_cast <Exception*> (NULL) /* exc_ */);
                }
            }
        }

        return;
    }

    // 'ready_for_output' is 'true' at this point. 
    if (res == AsyncIoResult::Again)
        ready_for_output = false;

    mt_unlocks (mutex) closeIfNeeded (true /* deferred_event */, unlock);
}

void
ImmediateConnectionSender::flush ()
{
    mutex.lock ();
    mt_unlocks (mutex) doFlush (true /* unlock */);
}

mt_mutex (mutex) void
ImmediateConnectionSender::flush_locked ()
{
    doFlush (false /* unlock */);
}

void
ImmediateConnectionSender::closeAfterFlush ()
{
    mutex.lock ();
    close_after_flush = true;
    mt_unlocks (mutex) closeIfNeeded (true /* deferred_event */, true /* unlock */);
}

void
ImmediateConnectionSender::close ()
{
    mutex.lock ();
    if (closed) {
        mutex.unlock ();
        return;
    }
    closed = true;
    mutex.unlock ();

    fireClosed_deferred (&deferred_reg, NULL /* exc_buf */);
    if (frontend) {
        frontend.call_deferred (&deferred_reg,
                                frontend->closed,
                                NULL /* extra_ref_data */,
                                static_cast <Exception*> (NULL) /* exc_ */);
    }
}

mt_mutex (mutex) bool
ImmediateConnectionSender::isClosed_locked ()
    { return closed; }

mt_mutex (mutex) SenderState
ImmediateConnectionSender::getSenderState_locked ()
    { return conn_sender_impl.getSenderState(); }

ImmediateConnectionSender::ImmediateConnectionSender (EmbedContainer * const embed_container)
    : Sender (embed_container),
    #ifdef LIBMARY_WIN32_IOCP
      conn_sender_impl (CbDesc<Overlapped::IoCompleteCallback> (&outputIoComplete, this, this)),
    #endif
      closed (false),
      close_after_flush (false),
      ready_for_output (true)
{
    conn_sender_impl.init (&frontend, this /* sender */, &deferred_reg);
}

ImmediateConnectionSender::~ImmediateConnectionSender ()
{
    mutex.lock();
    conn_sender_impl.release ();
    mutex.unlock();
}

}

