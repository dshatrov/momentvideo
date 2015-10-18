/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <libmary/util_dev.h>
#include <libmary/log.h>

#include <libmary/connection_sender_impl.h>


//#warning "TODO Don't hold the mutex while in write*() syscall."

//#warning "TODO For datagram mode, don't set ready_for_output to false on EAGAIN. Probably make an extra explicit flag for this."


namespace M {

static LogGroup libMary_logGroup_send    ("send",    LogLevel::I);
static LogGroup libMary_logGroup_writev  ("writev",  LogLevel::I);
static LogGroup libMary_logGroup_close   ("close",   LogLevel::I);
static LogGroup libMary_logGroup_hexdump ("hexdump", LogLevel::I);
static LogGroup libMary_logGroup_mwritev ("sender_impl_mwritev", LogLevel::I);

#ifdef LIBMARY_WIN32_IOCP
ConnectionSenderImpl::SenderOverlapped::~SenderOverlapped ()
{
  // At this point, ConnectionSenderImpl object is no more.
  // We simply release all remaining data.

    // Synchronizing in the absense of *Sender::mutex.
    full_memory_barrier ();

    SenderMessageEntry_PendingMessageList::iterator msg_iter (pending_msg_list);
    while (!msg_iter.done()) {
        SenderMessageEntry * const msg_entry = msg_iter.next ();
        msg_entry->release ();
    }
}
#endif

void
ConnectionSenderImpl::setSenderState (SenderState const new_state)
{
    if (new_state == sender_state)
        return;

    logD (send, _func, "Send state: ", (unsigned) new_state);

    sender_state = new_state;

    if (sender)
        sender->fireSenderStateChanged_deferred (deferred_reg, new_state);

    if (frontend && (*frontend)) {
        frontend->call_deferred (deferred_reg,
                                 (*frontend)->senderStateChanged,
                                 NULL /* extra_ref_data */,
                                 new_state);
    }
}

#ifdef LIBMARY_WIN32_IOCP
//#warning TODO Communicate the number of bytes transferred here and compare it with the expected value.
//#warning      Can it be less during normal operation (without I/O errors)?
void
ConnectionSenderImpl::outputComplete ()
{
    overlapped_pending = false;

    {
        SenderMessageEntry_PendingMessageList::iterator msg_iter (sender_overlapped->pending_msg_list);
        while (!msg_iter.done()) {
            SenderMessageEntry * const msg_entry = msg_iter.next ();
            if (msg_entry == msg_list.getFirst())
                break;

//#warning This code assumes that outputComplete() means that the messages has been sent
//#warning in full, which is not necessarily true because of LIBMARY__IOV_MAX limit.
            sender_overlapped->pending_msg_list.remove (msg_entry);
            msg_entry->release ();
        }
    }

    if (sender_state == SenderState::ConnectionOverloaded)
        setSenderState (SenderState::ConnectionReady);

    overloaded = false;
}
#endif

AsyncIoResult
ConnectionSenderImpl::sendPendingMessages ()
    mt_throw ((IoException,
               InternalException))
{
    logD (send, _func_);

  #ifdef LIBMARY_WIN32_IOCP
    if (overlapped_pending)
        return AsyncIoResult::Again;
  #endif

    logD (send, _func, "calling sendPendingMessages_writev()");
    return sendPendingMessages_writev ();
}

#ifdef LIBMARY_ENABLE_MWRITEV
void
ConnectionSenderImpl::sendPendingMessages_fillIovs (Count        * const ret_num_iovs,
                                                    struct iovec * const ret_iovs,
                                                    Count          const max_iovs)
{
    if (!gotDataToSend()) {
        logD (mwritev, _func, "no data to send");

        if (sender_state == SenderState::ConnectionOverloaded)
            setSenderState (SenderState::ConnectionReady);

        overloaded = false;
        return;
    }

    sendPendingMessages_vector_fill (ret_num_iovs,
                                     ret_iovs,
                                     LIBMARY__IOV_MAX <= max_iovs ? LIBMARY__IOV_MAX : max_iovs /* num_iovs */);
}

void
ConnectionSenderImpl::sendPendingMessages_react (AsyncIoResult res,
                                                 Size          num_written)
{
    if (res == AsyncIoResult::Again) {
        logD (mwritev, _func, "connection overloaded");

        if (sender_state == SenderState::ConnectionReady)
            setSenderState (SenderState::ConnectionOverloaded);

        overloaded = true;
        return;
    }

    sendPendingMessages_vector_react (num_written);

    if (!gotDataToSend()) {
        logD (mwritev, _func, "connection ready");

        if (sender_state == SenderState::ConnectionOverloaded)
            setSenderState (SenderState::ConnectionReady);

        overloaded = false;
        return;
    }
}
#endif // LIBMARY_ENABLE_MWRITEV

AsyncIoResult
ConnectionSenderImpl::sendPendingMessages_writev ()
    mt_throw ((IoException,
               InternalException))
{
    logD (send, _func, "msg_list len: ", msg_list.countNumElements());

    for (;;) {
        if (!gotDataToSend()) {
            logD (send, _func, "no data to send");

            if (sender_state == SenderState::ConnectionOverloaded)
                setSenderState (SenderState::ConnectionReady);

            overloaded = false;
            return AsyncIoResult::Normal;
        }

        Count num_iovs = 0;

      #ifdef LIBMARY_WIN32_IOCP
        Size num_bytes = 0;
        WSABUF buffers [LIBMARY__IOV_MAX];
      #else
        struct iovec iovs [LIBMARY__IOV_MAX];
      #endif

        if (!sendPendingMessages_vector_fill (&num_iovs,
                                            #ifdef LIBMARY_WIN32_IOCP
                                              &num_bytes,
                                              buffers,
                                            #else
                                              iovs,
                                            #endif
                                              LIBMARY__IOV_MAX /* num_iovs */))
        {
            return AsyncIoResult::Error;
        }

        if (num_iovs > LIBMARY__IOV_MAX) {
            logF_ (_func, "num_iovs: ", num_iovs, ", LIBMARY__IOV_MAX: ", LIBMARY__IOV_MAX);
            assert (0);
        }

#if 0
        // Dump of all iovs (IOCP).
        if (logLevelOn (hexdump, LogLevel::Debug)) {
            logLock ();
            log_locked_ (libMary_logGroup_writev.getLogLevel(), _func, "iovs:");
            for (Count i = 0; i < num_iovs; ++i) {
                logD_locked (writev, "    #", i, ": 0x", fmt_hex, (UintPtr) buffers [i].buf, ": ", fmt_def, buffers [i].len);
                hexdump (logs, ConstMemory ((Byte const *) buffers [i].buf, buffers [i].len));
            }
            logUnlock ();
        }
#endif

#if 0
        // Dump of all iovs.
        if (logLevelOn (hexdump, LogLevel::Debug)) {
            logLock ();
            log_locked_ (libMary_logGroup_writev.getLogLevel(), _func, "iovs:");
            for (Count i = 0; i < num_iovs; ++i) {
                logD_locked (writev, "    #", i, ": 0x", fmt_hex, (UintPtr) iovs [i].iov_base, ": ", fmt_def, iovs [i].iov_len);
                hexdump (logs, ConstMemory ((Byte const *) iovs [i].iov_base, iovs [i].iov_len));
            }
            logUnlock ();
        }
#endif

        Size num_written = 0;
        if (num_iovs) {
            logD (send, _func, "writev: num_iovs: ", num_iovs);
          #ifdef LIBMARY_WIN32_IOCP
            sender_overlapped->ref ();
          #endif
            AsyncIoResult res;
            if (datagram_mode) {
                res = conn->sendVectorTo (datagram_sendto_addr,
                                        #ifdef LIBMARY_WIN32_IOCP
                                          sender_overlapped,
                                          buffers,
                                        #else
                                          iovs,
                                        #endif
                                          num_iovs,
                                          &num_written);
            } else {
                res = conn->writev (
                                  #ifdef LIBMARY_WIN32_IOCP
                                    sender_overlapped,
                                    buffers,
                                  #else
                                    iovs,
                                  #endif
                                    num_iovs,
                                    &num_written);
            }
            if (res == AsyncIoResult::Error) {
              #ifdef LIBMARY_WIN32_IOCP
                sender_overlapped->unref ();
              #endif
                if (!exc) {
                    logD_ (_this_func, "WARNING: exc is null");
                    exc_throw (InternalException, InternalException::BackendMalfunction);
                }
                return AsyncIoResult::Error;
            }

          #ifdef LIBMARY_WIN32_IOCP
            num_written = num_bytes;
            overlapped_pending = true;
          #endif

            if (res == AsyncIoResult::Again) {
                if (sender_state == SenderState::ConnectionReady)
                    setSenderState (SenderState::ConnectionOverloaded);

                logD (send, _func, "connection overloaded");
                overloaded = true;

                sendPendingMessages_vector_react (
              #ifdef LIBMARY_WIN32_IOCP
                        // AsyncIoResult::Again has different semantics for IOCP.
                        num_written
              #else
                        0 /* num_written */
              #endif
                        );
                return AsyncIoResult::Again;
            }

            if (res == AsyncIoResult::Eof) {
                logD (close, _func, "Eof, num_iovs: ", num_iovs);
                return AsyncIoResult::Eof;
            }

            // Normal_Again is not handled specially here yet.

            // Note that we can get "num_written == 0" here in two cases:
            //   a) writev() syscall returned EINTR;
            //   b) there was nothing to send.
            // That's why we do not do a usual EINTR loop here.
        } // if (num_iovs)

        sendPendingMessages_vector_react (num_written);

      #ifdef LIBMARY_WIN32_IOCP
        // We can't reuse OVERLAPPED until we get completion notification.
        return AsyncIoResult::Again;
      #endif
    } // for (;;)

    unreachable();
    return AsyncIoResult::Normal;
}

Result
ConnectionSenderImpl::sendPendingMessages_vector_fill (Count        * const mt_nonnull ret_num_iovs,
                                                     #ifdef LIBMARY_WIN32_IOCP
                                                       Size         * const mt_nonnull ret_num_bytes,
                                                       WSABUF       * const mt_nonnull iov_elems,
                                                     #else
                                                       struct iovec * const mt_nonnull iov_elems,
                                                     #endif
                                                       Count          const num_iovs)
{
    logD (writev, _func_);

    *ret_num_iovs = 0;
  #ifdef LIBMARY_WIN32_IOCP
    *ret_num_bytes = 0;
  #endif

    SenderMessageEntry *msg_entry = msg_list.getFirst ();
    if (mt_unlikely (!msg_entry)) {
        logD (writev, _func, "message queue is empty");
        return Result::Success;
    }

    IovArray arr (iov_elems, num_iovs);
    while (msg_entry) {
        {
            Uint64 seek_offset = 0;
            bool const has_seek = msg_entry->getSeek (&seek_offset);
            if (has_seek && arr.i > 0)
                break;

            if (has_seek) {
                msg_entry->seekDone ();
                if (!conn->seekBeg (seek_offset)) {
                    logE_ (_this_func, "seekBeg() failed");
                    return Result::Failure;
                }
            }
        }

        SenderMessageEntry * const next_msg_entry = msg_list.getNext (msg_entry);

        msg_entry->fillIovs (&arr, num_iovs - arr.i);

        if (arr.i >= num_iovs)
            break;

        if (datagram_mode && arr.i > 0)
            break;

        msg_entry = next_msg_entry;
    } // while (msg_entry)

    *ret_num_iovs  = arr.i;
  #ifdef LIBMARY_WIN32_IOCP
    *ret_num_bytes = arr.num_bytes;
  #endif

    return Result::Success;
}

void
ConnectionSenderImpl::sendPendingMessages_vector_react (Size num_written)
{
    logD (writev, _func_);

    SenderMessageEntry *msg_entry = msg_list.getFirst ();
    if (mt_unlikely (!msg_entry)) {
        logD (writev, _func, "message queue is empty");
        return;
    }

    while (msg_entry) {
        SenderMessageEntry * const next_msg_entry = msg_list.getNext (msg_entry);

      #ifdef LIBMARY_WIN32_IOCP
        bool const is_empty = msg_entry->isEmpty();
        if (!is_empty) {
            if (sender_overlapped->pending_msg_list.getLast() != msg_entry)
                sender_overlapped->pending_msg_list.append (msg_entry);
        }
      #endif

        Size accepted = 0;
        bool const msg_sent_completely = msg_entry->react (num_written, &accepted);
        assert (num_written >= accepted);
        num_written -= accepted;

        if (msg_sent_completely) {
          // This is the only place where messages are removed from the queue.

            msg_list.remove (msg_entry);

          #ifdef LIBMARY_WIN32_IOCP
            if (is_empty)
          #endif
            {
                msg_entry->release ();
            }

            --num_msg_entries;
            if (mt_unlikely (sender_state == SenderState::QueueSoftLimit ||
                             sender_state == SenderState::QueueHardLimit))
            {
                if (num_msg_entries < soft_msg_limit) {
                    if (overloaded)
                        setSenderState (SenderState::ConnectionOverloaded);
                    else
                        setSenderState (SenderState::ConnectionReady);
                } else
                if (num_msg_entries < hard_msg_limit)
                    setSenderState (SenderState::QueueSoftLimit);
            }
        } else {
            assert (gotDataToSend());
            break;
        }

        msg_entry = next_msg_entry;
    } // while (msg_entry)

    assert (num_written == 0);
}

void
ConnectionSenderImpl::queueMessage (SenderMessageEntry * const mt_nonnull msg_entry)
{
    bool const has_seek = msg_entry->getSeek (NULL /* ret_offset */);

    if (msg_entry->isEmpty() && !has_seek)
        return;

    // If new entry has seek, and prv entry is seek-only, then remove prv entry.
    if (has_seek) {
        if (SenderMessageEntry * const prv_entry = msg_list.getLast()) {
            if (prv_entry->isEmpty()) {
                assert (prv_entry->getSeek (NULL /* ret_offset */));
                msg_list.remove (prv_entry);
                prv_entry->release ();
            }
        }
    }

    ++num_msg_entries;
    if (num_msg_entries >= hard_msg_limit)
        setSenderState (SenderState::QueueHardLimit);
    else
    if (num_msg_entries >= soft_msg_limit)
        setSenderState (SenderState::QueueSoftLimit);

    msg_list.append (msg_entry);
}

ConnectionSenderImpl::ConnectionSenderImpl (
                                          #ifdef LIBMARY_WIN32_IOCP
                                            CbDesc<Overlapped::IoCompleteCallback> const &io_complete_cb,
                                          #endif
                                            bool const datagram_mode)
    : datagram_mode      (datagram_mode),
      conn               (NULL),
      soft_msg_limit     (1024),
      hard_msg_limit     (4096),
    #ifdef LIBMARY_WIN32_IOCP
      overlapped_pending (false),
    #endif
      sender_state       (SenderState::ConnectionReady),
      overloaded         (false),
      num_msg_entries    (0)
{
  #ifdef LIBMARY_WIN32_IOCP
    sender_overlapped = grab (new (std::nothrow) SenderOverlapped);
    sender_overlapped->io_complete_cb = io_complete_cb;
  #endif
}

void
ConnectionSenderImpl::release ()
{
    SenderMessageEntry_MessageList::iter iter (msg_list);
    while (!msg_list.iter_done (iter)) {
        SenderMessageEntry * const msg_entry = msg_list.iter_next (iter);
      #ifdef LIBMARY_WIN32_IOCP
        // msg_list and pending_msg_list cannot overlap for more than one MessageEntry,
        // the first for msg_list and the last for pending_msg_list.
        if (msg_entry != sender_overlapped->pending_msg_list.getLast())
      #endif
            msg_entry->release();
    }
    msg_list.clear ();
}

}

