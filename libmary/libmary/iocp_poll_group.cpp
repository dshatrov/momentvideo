/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/util_time.h>
#include <libmary/log.h>

#include <libmary/iocp_poll_group.h>


namespace M {

static LogGroup libMary_logGroup_iocp ("iocp", LogLevel::I);

mt_throws Result
IocpPollGroup::doActivate (HANDLE const handle)
{
    logD (iocp, _this_func_);

    // Memory barrier for mt_const data - likely unnecessary.
    mutex.lock ();
    mutex.unlock ();

    HANDLE const res = CreateIoCompletionPort (handle,
                                               iocp_handle,
                                               (ULONG_PTR) this /* CompletionKey */,
                                               0 /* NumberOfConcurrentThreads */);
    if (!res) {
        exc_throw (Win32Exception, GetLastError());
        return Result::Failure;
    }

    return Result::Success;
}

//#warning TODO auto_remove
mt_throws PollGroup::PollableKey
IocpPollGroup::addPollable (CbDesc<Pollable> const &pollable,
                            bool             const /* auto_remove */)
{
    logD (iocp, _this_func_);

    HANDLE const handle = (HANDLE) pollable->getFd (pollable.cb_data);

    if (!doActivate (handle))
        return NULL;

    PollableKey key (this /* any non-null ptr */);
    key.handle = handle;
    return key;
}

void
IocpPollGroup::removePollable (PollableKey const mt_nonnull key)
{
  #if 0
  // Kept for reference

    // CancelIo works only for I/O operations issued by the current thread.
    // CancelIoEx is Vista+.
    if (!CancelIo (pollable_entry->handle))
        logE_ (_this_func, "CancelIoEx() failed: ", win32ErrorToString (GetLastError()));
  #endif

  // No-op
}

mt_throws Result
IocpPollGroup::poll (Uint64 const timeout_microsec)
{
    logD (iocp, _this_func, "timeout_microsec: ", timeout_microsec, ", (0x", fmt_hex, timeout_microsec, ")");

    Time const start_microsec = getTimeMicroseconds ();

    bool first = true;
    for (;;) {
        Time cur_microsec = first ? (first = false, start_microsec) : getTimeMicroseconds ();
        if (cur_microsec < start_microsec)
            cur_microsec = start_microsec;

        Time const elapsed_microsec = cur_microsec - start_microsec;

        DWORD poll_timeout_millisec;
        if (!got_deferred_tasks) {
            if (timeout_microsec != (Uint64) -1) {
                if (timeout_microsec > elapsed_microsec) {
                    Time const tmp_timeout = (timeout_microsec - elapsed_microsec) / 1000;
                    poll_timeout_millisec = (DWORD) tmp_timeout;
                    if (poll_timeout_millisec != tmp_timeout || poll_timeout_millisec == INFINITE)
                        poll_timeout_millisec = 3600000 /* 1 hour */;

                    if (poll_timeout_millisec == 0)
                        poll_timeout_millisec = 1;
                } else {
                    poll_timeout_millisec = 0;
                }
            } else {
                poll_timeout_millisec = INFINITE;
            }
        } else {
            poll_timeout_millisec = 0;
        }

        mutex.lock ();
        if (triggered || poll_timeout_millisec == 0) {
            block_trigger_pipe = true;
            poll_timeout_millisec = 0;
        } else {
            block_trigger_pipe = false;
        }
        mutex.unlock ();

        int err = ERROR_SUCCESS;
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        OVERLAPPED *sys_overlapped = NULL;
        // TODO Loop with zero timeout until there's no ready handles, then go for the real timeout.
        //
        // GetQueuedCompletionStatusEw allows to dequeue multiple events at once, but it is Vista+.
        logD (iocp, _this_func, "calling GetQueuedCompletionStatus(), "
              "poll_timeout_millisec: ", poll_timeout_millisec, " (0x", fmt_hex, poll_timeout_millisec, ")");
        if (!GetQueuedCompletionStatus (iocp_handle,
                                        &bytes_transferred,
                                        &completion_key,
                                        &sys_overlapped,
                                        poll_timeout_millisec))
        {
            err = GetLastError();
            logD (iocp, _func, "GetQueuedCompletionStatus error code: ", win32ErrorToString (err));
            if (!sys_overlapped) {
                if (err == WAIT_TIMEOUT) {
                    logD (iocp, _this_func, "WAIT_TIMEOUT");
                } else {
                    logF (iocp, _this_func, "GetQueuedCompletionStatus() failed: ", win32ErrorToString (err));
                    exc_throw (Win32Exception, err);
                    return Result::Failure;
                }
            } else {
                assert (completion_key == (ULONG_PTR) this);
            }
        } else {
            assert (completion_key == (ULONG_PTR) this);
        }

        mutex.lock ();
        block_trigger_pipe = true;
        bool const was_triggered = triggered;
        triggered = false;
        mutex.unlock ();

        got_deferred_tasks = false;

        logD (iocp, _this_func, "calling pollIterationBegin");
        if (frontend)
            frontend.call (frontend->pollIterationBegin);

        bool trigger_event = false;
        if ((void*) sys_overlapped == (void*) this) {
            logD (iocp, _this_func, "trigger_event");
            trigger_event = true;
        } else
        if (sys_overlapped) {
            Overlapped * const overlapped = static_cast <Overlapped*> (sys_overlapped);
            WSAException exc_ (err);
            if (overlapped->io_complete_cb) {
                overlapped->io_complete_cb.call_ ((err == ERROR_SUCCESS ? NULL : &exc_),
                                                  overlapped,
                                                  (Size) bytes_transferred);
            }
            overlapped->unref ();
        }

        logD (iocp, _this_func, "calling pollIterationEnd");
        if (frontend) {
            bool extra_iteration_needed = false;
            frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
            if (extra_iteration_needed) {
                got_deferred_tasks = true;
                logD (iocp, _this_func, "extra_iteration_needed");
            }
        }

        if (trigger_event || was_triggered) {
            logD (iocp, _this_func, "trigger break");
            break;
        }

        if (elapsed_microsec >= timeout_microsec) {
            logD (iocp, _this_func, "timeout break");
            break;
        }
    }

    logD (iocp, _this_func, "done");
    return Result::Success;
}

mt_throws Result
IocpPollGroup::trigger ()
{
    logD (iocp, _this_func_);

    mutex.lock ();
    if (triggered) {
        mutex.unlock ();
        logD (iocp, _this_func, "already triggered");
        return Result::Success;
    }
    triggered = true;

    if (block_trigger_pipe) {
        mutex.unlock ();
        return Result::Success;
    }
    mutex.unlock ();

    if (!PostQueuedCompletionStatus (iocp_handle,
                                     0 /* dwNumberOfBytesTransferred */,
                                     (ULONG_PTR) this /* dwCompletionKey */,
                                     (LPOVERLAPPED) (void*) this))
    {
        DWORD const err = GetLastError();
        logF (iocp, _this_func, "PostQueuedCompletionStatus() failed: ", win32ErrorToString (err));
        exc_throw (Win32Exception, err);
        return Result::Failure;
    }

    logD (iocp, _this_func, "done");
    return Result::Success;
}

mt_const mt_throws Result
IocpPollGroup::open ()
{
    logD (iocp, _this_func_);

    assert (iocp_handle == NULL);
    iocp_handle = CreateIoCompletionPort (INVALID_HANDLE_VALUE /* FileHandle */,
                                          NULL                 /* ExistingCompletionPort */,
                                          0                    /* CompletionKey */,
                                          1                    /* NumberOfConcurrentThreads */);
    if (iocp_handle == NULL) {
        DWORD const err = GetLastError();
        logF (iocp, _this_func, "CreateIoCompletionPort() failed: ", win32ErrorToString (err));
        exc_throw (Win32Exception, err);
        return Result::Failure;
    }

    return Result::Success;
}

IocpPollGroup::IocpPollGroup (EmbedContainer * const embed_container)
    : ActivePollGroup (embed_container),
      iocp_handle (NULL),
      triggered (false),
      block_trigger_pipe (true),
      got_deferred_tasks (true)
{
    logD (iocp, _this_func_);
}

IocpPollGroup::~IocpPollGroup ()
{
    logD (iocp, _this_func_);

    if (iocp_handle != NULL) {
        if (!CloseHandle (iocp_handle)) {
            logF (iocp, _this_func, "CloseHandle() failed: ", win32ErrorToString (GetLastError()));
        }
        iocp_handle = NULL;
    }
}

}

