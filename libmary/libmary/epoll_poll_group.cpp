/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#ifdef LIBMARY_ENABLE_KQUEUE
  #include <sys/types.h>
  #include <sys/event.h>
  #include <sys/time.h>
#else
  #include <sys/epoll.h>
#endif
#include <unistd.h>
#include <errno.h>

#include <libmary/util_time.h>
#include <libmary/util_posix.h>
#include <libmary/log.h>

#include <libmary/epoll_poll_group.h>


namespace M {

static LogGroup libMary_logGroup_epoll ("epoll", LogLevel::I);

mt_mutex (mutex) void
EpollPollGroup::processPollableDeletionQueue ()
{
    PollableDeletionQueue::iterator iter (pollable_deletion_queue);
    while (!iter.done()) {
        PollableEntry * const pollable_entry = iter.next ();
        delete pollable_entry;
    }
    pollable_deletion_queue.clear ();
}

mt_throws PollGroup::PollableKey
EpollPollGroup::addPollable (CbDesc<Pollable> const &pollable_cb,
                             bool const auto_remove)
{
    PollableEntry * const pollable_entry = new (std::nothrow) PollableEntry;
    assert (pollable_entry);

    logD (epoll, _func, "0x", fmt_hex, (UintPtr) this, ": "
          "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry, ", "
          "cb_data: 0x", fmt_hex, (UintPtr) pollable_cb.cb_data, ", "
          "auto_remove ", auto_remove);

    pollable_entry->valid = true;
    pollable_entry->epoll_poll_group = this;
    pollable_entry->pollable = pollable_cb;
    // We're making an unsafe call, assuming that the pollable is available.
    pollable_entry->fd = pollable_cb->getFd (pollable_cb.cb_data);
    pollable_entry->auto_remove = auto_remove;

    if (auto_remove && pollable_cb.guard_obj) {
        pollable_entry->del_sbn = pollable_cb.guard_obj->addDeletionCallback (
                CbDesc<Object::DeletionCallback> (pollableDeletionCallback,
                                                  pollable_entry,
                                                  this));
    }

    mutex.lock ();
    pollable_list.append (pollable_entry);
    mutex.unlock ();

    if (!doActivate (pollable_entry))
        goto _failure;

    return pollable_entry;

_failure:
    mutex.lock ();
    pollable_list.remove (pollable_entry);
    mutex.unlock ();
    delete pollable_entry;

    return NULL;
}

mt_throws Result
EpollPollGroup::doActivate (PollableEntry * const mt_nonnull pollable_entry)
{
    logD (epoll, _func, "0x", fmt_hex, (UintPtr) this, ": "
          "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

  #ifdef LIBMARY_ENABLE_KQUEUE
    // TODO Single kevent() syscall.

    {
        struct kevent change;
        change.ident  = pollable_entry->fd;
        change.filter = EVFILT_READ;
        change.flags  = EV_ADD | EV_CLEAR;
        change.fflags = 0;
        change.data   = 0;
        change.udata  = pollable_entry;

        struct kevent dummy_eventlist [1];

        int const res = kevent (efd, &change, 1 /* nchanges */, dummy_eventlist, 0 /* nevents */, NULL /* timeout */);
        if (res == -1) {
            logE_ (_func, "kevent() (EVFILT_READ) failed: ", errnoString (errno));
        } else {
            if (res != 0)
                logE_ (_func, "kevent() (EVFILT_READ): unexpected return value: ", res);
        }
    }

    {
        struct kevent change;
        change.ident  = pollable_entry->fd;
        change.filter = EVFILT_WRITE;
        change.flags  = EV_ADD | EV_CLEAR;
        change.fflags = 0;
        change.data   = 0;
        change.udata  = pollable_entry;

        struct kevent dummy_eventlist [1];

        int const res = kevent (efd, &change, 1 /* nchanges */, dummy_eventlist, 0 /* nevents */, NULL /* timeout */);
        if (res == -1) {
            logE_ (_func, "kevent() (EVFILT_WRITE) failed: ", errnoString (errno));
        } else {
            if (res != 0)
                logE_ (_func, "kevent() (EVFILT_WRITE): unexpected return value: ", res);
        }
    }
  #else
    struct epoll_event event;
    event.events =
              EPOLLET | EPOLLIN | EPOLLOUT 
          #ifndef LIBMARY_PLATFORM_ANDROID
            | EPOLLRDHUP
          #endif
            ;
    event.data.u64 = 0; // For valgrind.
    event.data.ptr = pollable_entry;
    int const res = epoll_ctl (efd, EPOLL_CTL_ADD, pollable_entry->fd, &event);
    if (res == -1) {
        exc_throw (PosixException, errno);
        logE_ (_func, "epoll_ctl() failed: ", errnoString (errno));
        return Result::Failure;
    }

    if (res != 0) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        logE_ (_func, "epoll_ctl(): unexpected return value: ", res);
        return Result::Failure;
    }
  #endif

    if (!trigger ()) {
        logF_ (_func, "trigger() failed: ", exc->toString());
        return Result::Failure;
    }

    // We've added a new pollable to the poll group. We don't yet know its state
    // and we use edge-triggered events, hence we should assume initially that
    // input and output events should be reported without waiting for an edge.
    // Epoll appears to handle this for us, delivering ET I/O events which occured
    // before EPOLL_CTL_ADD.

    return Result::Success;
}

void
EpollPollGroup::pollableDeletionCallback (void * const _pollable_entry)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    EpollPollGroup * const self = pollable_entry->epoll_poll_group;

    self->removePollable_fromDeletionCb (pollable_entry);
}

void
EpollPollGroup::removePollable_fromDeletionCb (PollableEntry * const mt_nonnull pollable_entry)
{
    assert (pollable_entry->auto_remove);
    mutex.lock ();
    if (!pollable_entry->valid) {
        // If invalid, then invalidated by _fromPoll(), which means that
        // EPOLL_DEL is to be called properly.
        //
        // Pollable was not scheduled for deletion to keep the object available
        // here.

        pollable_list.remove (pollable_entry);
        pollable_deletion_queue.append (pollable_entry);
        mutex.unlock ();
        return;
    }
    pollable_entry->valid = false;
    mutex.unlock ();

    // At this point, it is possible that EPOLL_DEL will not be called immediately
    // after EPOLLERR/HUP/RDHUP, allowing for several iterations of epoll_wait
    // with fd in error state.

    // It is important to do EPOLL_CTL_DEL first, schedule pollable_entry for
    // deletion second.
    doRemovePollable (pollable_entry);

    mutex.lock ();
    pollable_list.remove (pollable_entry);
    pollable_deletion_queue.append (pollable_entry);
    mutex.unlock ();
}

void
EpollPollGroup::removePollable_fromPoll (PollableEntry * const mt_nonnull pollable_entry)
{
    assert (pollable_entry->auto_remove);
    mutex.lock ();
    if (!pollable_entry->valid) {
      // pollable_entry has been scheduled for deletion by _fromDeletionCb(),
      // EPOLL_CTL_DEL has already been called.
        mutex.unlock ();
        return;
    }
    pollable_entry->valid = false;
    mutex.unlock ();

    Ref<Object> const obj = pollable_entry->pollable.getWeakRef().getRef ();
    if (obj) {
        obj->removeDeletionCallback (pollable_entry->del_sbn);
        // _fromDeletionCb() will not be called for sure.
    } else
    if (pollable_entry->pollable.getWeakRef().isValid()) {
      // pollable_entry will be scheduled for deletion by _fromDeletionCb().
        doRemovePollable (pollable_entry);
        return;
    }

    doRemovePollable (pollable_entry);

    mutex.lock ();
    pollable_list.remove (pollable_entry);
    pollable_deletion_queue.append (pollable_entry);
    mutex.unlock ();
}

void
EpollPollGroup::doRemovePollable (PollableEntry * const mt_nonnull pollable_entry)
{
  #ifdef LIBMARY_ENABLE_KQUEUE
    // TODO Single kevent() syscall.

    {
        struct kevent change;
        change.ident  = pollable_entry->fd;
        change.filter = EVFILT_READ;
        change.flags  = EV_DELETE;
        change.fflags = 0;
        change.data   = 0;
        change.udata  = pollable_entry;

        struct kevent dummy_eventlist [1];

        int const res = kevent (efd, &change, 1 /* nchanges */, dummy_eventlist, 0 /* nevents */, NULL /* timeout */);
        if (res == -1) {
            logF_ (_func, "kevent() (EVFILT_READ) failed: ", errnoString (errno));
        } else {
            if (res != 0)
                logF_ (_func, "kevent() (EVFILT_READ): unexpected return value: ", res);
        }
    }

    {
        struct kevent change;
        change.ident  = pollable_entry->fd;
        change.filter = EVFILT_WRITE;
        change.flags  = EV_DELETE;
        change.fflags = 0;
        change.data   = 0;
        change.udata  = pollable_entry;

        struct kevent dummy_eventlist [1];

        int const res = kevent (efd, &change, 1 /* nchanges */, dummy_eventlist, 0 /* nevents */, NULL /* timeout */);
        if (res == -1) {
            logF_ (_func, "kevent() (EVFILT_WRITE) failed: ", errnoString (errno));
        } else {
            if (res != 0)
                logF_ (_func, "kevent() (EVFILT_WRITE): unexpected return value: ", res);
        }
    }
  #else
    int const res = epoll_ctl (efd, EPOLL_CTL_DEL, pollable_entry->fd, NULL /* event */);
    if (res == -1) {
        logF_ (_func, "epoll_ctl() failed: ", errnoString (errno));
    } else
    if (res != 0)
        logF_ (_func, "epoll_ctl(): unexpected return value: ", res);
  #endif
}

void
EpollPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> ((void*) key);
    assert (!pollable_entry->auto_remove);

    logD (epoll, _func, "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry);

    doRemovePollable (pollable_entry);

    mutex.lock ();
    // 'valid' is unused here (it's not used when !auto_remove)
    pollable_entry->valid = false;
    pollable_list.remove (pollable_entry);
    pollable_deletion_queue.append (pollable_entry);
    mutex.unlock ();
}

mt_throws Result
EpollPollGroup::poll (Uint64 const timeout_microsec)
{
    logD (epoll, _func, "timeout: ", timeout_microsec);

    Time const start_microsec = getTimeMicroseconds ();

  #ifdef LIBMARY_ENABLE_KQUEUE
    struct kevent dummy_changelist [1];
    struct kevent eventlist [4096];
  #else
    struct epoll_event events [4096];
  #endif

    bool first = true;
    for (;;) {
        Time cur_microsec = first ? (first = false, start_microsec) : getTimeMicroseconds ();
        if (cur_microsec < start_microsec)
            cur_microsec = start_microsec;

        Time const elapsed_microsec = cur_microsec - start_microsec;

      #ifdef LIBMARY_ENABLE_KQUEUE
        bool infinite_timeout = false;
        Time cur_timeout = 0;
        if (!got_deferred_tasks) {
            if (timeout_microsec != (Uint64) -1) {
                if (timeout_microsec > elapsed_microsec)
                    cur_timeout = timeout_microsec - elapsed_microsec;
            } else {
                infinite_timeout = true;
            }
        }
      #else
        int cur_timeout;
        if (!got_deferred_tasks) {
            if (timeout_microsec != (Uint64) -1) {
                if (timeout_microsec > elapsed_microsec) {
                    Uint64 const tmp_timeout = (timeout_microsec - elapsed_microsec) / 1000;
                    cur_timeout = (int) tmp_timeout;
                    if ((Uint64) cur_timeout != tmp_timeout || cur_timeout < 0)
                        cur_timeout = Int_Max - 1;

                    if (cur_timeout == 0)
                        cur_timeout = 1;
                } else {
                    cur_timeout = 0;
                }
            } else {
                cur_timeout = -1;
            }
        } else {
            // We've got deferred tasks to process, hence we shouldn't block.
            cur_timeout = 0;
        }
      #endif

        mutex.lock ();
      #ifdef LIBMARY_ENABLE_KQUEUE
        if (triggered || (cur_timeout == 0 && !infinite_timeout))
      #else
        if (triggered || cur_timeout == 0)
      #endif
        {
            block_trigger_pipe = true;
            cur_timeout = 0;
          #ifdef LIBMARY_ENABLE_KQUEUE
            infinite_timeout = false;
          #endif
        } else {
            block_trigger_pipe = false;
        }
        mutex.unlock ();

      #ifdef LIBMARY_ENABLE_KQUEUE
        struct timespec timeout_spec;
        timeout_spec.tv_sec = cur_timeout / 1000000;
        timeout_spec.tv_nsec = (cur_timeout % 1000000) * 1000;

        int const nevents = kevent (efd,
                                    dummy_changelist,
                                    0 /* nchanges */,
                                    eventlist,
                                    sizeof (eventlist) / sizeof (eventlist [0]),
                                    infinite_timeout ? (struct timespec*) NULL : &timeout_spec);
        if (nevents == -1) {
            if (errno == EINTR)
                continue;

            exc_throw (PosixException, errno);
            logF_ (_func, "kevent() failed: ", errnoString (errno));
            return Result::Failure;
        }

        if (nevents < 0 || (Size) nevents > sizeof (eventlist) / sizeof (eventlist [0])) {
            logF_ (_func, "kevent(): unexpected return value: ", nevents);
            return Result::Failure;
        }
      #else
        int const nfds = epoll_wait (efd, events, sizeof (events) / sizeof (events [0]), cur_timeout);
        if (nfds == -1) {
            if (errno == EINTR)
                continue;

            exc_throw (PosixException, errno);
            logF_ (_func, "epoll_wait() failed: ", errnoString (errno));
            return Result::Failure;
        }

        if (nfds < 0 || (Size) nfds > sizeof (events) / sizeof (events [0])) {
            logF_ (_func, "epoll_wait(): unexpected return value: ", nfds);
            return Result::Failure;
        }
      #endif

      // Trigger optimization:
      //
      // After trigger() returns, two events MUST happen:
      //   1. pollIterationBegin() and pollIterationEnd() must be called,
      //      i.e. a full poll iteration must occur;
      //   2. poll() should return.
      //
      // For poll/select/epoll, we use pipes to implement trigger().
      // Writing and reading to/from a pipe is expensive. To reduce
      // the number of writes, we do the following:
      //
      // 1. write() makes sense only when we're blocked in epoll_wait(),
      //    i.e. when epoll_wait with a non-zero timeout is in progress.
      //    'block_trigger_pipe' is set to 'false' when we're (supposedly)
      //    blocked in epoll_wait().
      // 2. 'triggered' flag indicates that trigger() has been called.
      //    This flag should be checked and cleared right after epoll_wait
      //    to ensure that every trigger() results in a full extra poll
      //    iteration.

        // This lock() acts as a memory barrier which ensures that we see
        // valid contents of PollableEntry objects.
        mutex.lock ();
        block_trigger_pipe = true;
        bool const was_triggered = triggered;
        triggered = false;
        mutex.unlock ();

        got_deferred_tasks = false;

        if (frontend)
            frontend.call (frontend->pollIterationBegin);

        bool trigger_pipe_ready = false;
      #ifdef LIBMARY_ENABLE_KQUEUE
        for (int i = 0; i < nevents; ++i) {
            PollableEntry * const pollable_entry = static_cast <PollableEntry*> (eventlist [i].udata);
      #else
        for (int i = 0; i < nfds; ++i) {
            PollableEntry * const pollable_entry = static_cast <PollableEntry*> (events [i].data.ptr);
            uint32_t const epoll_event_flags = events [i].events;
      #endif
            Uint32 event_flags = 0;

            if (pollable_entry == NULL) {
              // Trigger pipe event.
              #ifdef LIBMARY_ENABLE_KQUEUE
                if (eventlist [i].filter == EVFILT_READ) {
                    trigger_pipe_ready = true;

                    if (   eventlist [i].flags & EV_EOF
                        || eventlist [i].flags & EV_ERROR)
                    {
                        logF_ (_func, "trigger pipe error, flags: ", eventlist [i].flags);
                    }
                } else {
                    logW_ (_func, "unexpected trigger pipe filter: ", eventlist [i].filter);
                }
              #else
                if (epoll_event_flags & EPOLLIN)
                    trigger_pipe_ready = true;

                if (epoll_event_flags & EPOLLOUT)
                    logW_ (_func, "unexpected EPOLLOUT event for trigger pipe");

                if (epoll_event_flags & EPOLLHUP   ||
                  #ifndef LIBMARY_PLATFORM_ANDROID
                    epoll_event_flags & EPOLLRDHUP ||
                  #endif
                    epoll_event_flags & EPOLLERR)
                {
                    logF_ (_func, "trigger pipe error: 0x", fmt_hex, epoll_event_flags);
                }
              #endif

                continue;
            }

          #ifdef LIBMARY_ENABLE_KQUEUE
            if (eventlist [i].filter == EVFILT_READ)
          #else
            if (epoll_event_flags & EPOLLIN)
          #endif
            {
                event_flags |= PollGroup::Input;
            }

          #ifdef LIBMARY_ENABLE_KQUEUE
            if (eventlist [i].filter == EVFILT_WRITE)
          #else
            if (epoll_event_flags & EPOLLOUT)
          #endif
            {
                event_flags |= PollGroup::Output;
            }

          #ifdef LIBMARY_ENABLE_KQUEUE
            if (eventlist [i].flags & EV_EOF)
          #else
            if (   epoll_event_flags & EPOLLHUP
              #ifndef LIBMARY_PLATFORM_ANDROID
                || epoll_event_flags & EPOLLRDHUP
              #endif
                )
          #endif
            {
                event_flags |= PollGroup::Hup;
            }

          #ifdef LIBMARY_ENABLE_KQUEUE
            if (eventlist [i].flags & EV_ERROR)
          #else
            if (epoll_event_flags & EPOLLERR)
          #endif
            {
                event_flags |= PollGroup::Error;
            }

            if (event_flags) {
                logD (epoll, _this_func, "pollable_entry: 0x", fmt_hex, (UintPtr) pollable_entry, ", "
                      "events: 0x", event_flags, " ",
                      (event_flags & PollGroup::Input  ? "I" : ""),
                      (event_flags & PollGroup::Output ? "O" : ""),
                      (event_flags & PollGroup::Error  ? "E" : ""),
                      (event_flags & PollGroup::Hup    ? "H" : ""));

                pollable_entry->pollable.call (
                        pollable_entry->pollable->processEvents, /*(*/ event_flags /*)*/);
            }

            if ((event_flags & (PollGroup::Hup | PollGroup::Error))
                && pollable_entry->auto_remove)
            {
                removePollable_fromPoll (pollable_entry);
            }
        } // for (;;) - for all events

        if (frontend) {
            bool extra_iteration_needed = false;
            frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
            if (extra_iteration_needed)
                got_deferred_tasks = true;
        }

        mutex.lock ();
        processPollableDeletionQueue ();
        mutex.unlock ();

        if (trigger_pipe_ready) {
            logD (epoll, _func, "trigger pipe break");

            if (!commonTriggerPipeRead (trigger_pipe [0])) {
                logF_ (_func, "commonTriggerPipeRead() failed: ", exc->toString());
                return Result::Failure;
            }
            break;
        }

        if (was_triggered) {
            logD (epoll, _func, "trigger break");
            break;
        }

        if (elapsed_microsec >= timeout_microsec) {
          // Timeout expired.
            break;
        }
    } // for (;;)

    return Result::Success;
}

mt_throws Result
EpollPollGroup::trigger ()
{
    mutex.lock ();
    if (triggered) {
        mutex.unlock ();
        return Result::Success;
    }
    triggered = true;

    if (block_trigger_pipe) {
        mutex.unlock ();
        return Result::Success;
    }
    mutex.unlock ();

    return commonTriggerPipeWrite (trigger_pipe [1]);
}

mt_const mt_throws Result
EpollPollGroup::open ()
{
  #ifdef LIBMARY_ENABLE_KQUEUE
    efd = kqueue ();
    if (efd == -1) {
        exc_throw (PosixException, errno);
        logF_ (_func, "kqueue() failed: ", errnoString (errno));
        return Result::Failure;
    }

    if (!posix_createNonblockingPipe (&trigger_pipe))
        return Result::Failure;

    {
        struct kevent change;
        change.ident  = trigger_pipe [0];
        change.filter = EVFILT_READ;
        change.flags  = EV_ADD | EV_CLEAR;
        change.fflags = 0;
        change.data   = 0;
        change.udata  = NULL;

        struct kevent dummy_eventlist [1];

        int const res = kevent (efd, &change, 1 /* nchanges */, dummy_eventlist, 0 /* nevents */, NULL /* timeout */);
        if (res == -1) {
            exc_throw (PosixException, errno);
            logF_ (_func, "kevent() (EVFILT_READ) failed: ", errnoString (errno));
            return Result::Failure;
        } else {
            if (res != 0) {
                exc_throw (InternalException, InternalException::BackendMalfunction);
                logF_ (_func, "kevent() (EVFILT_READ): unexpected return value: ", res);
                return Result::Failure;
            }
        }
    }
  #else
    efd = epoll_create (1 /* size, unused */);
    if (efd == -1) {
        exc_throw (PosixException, errno);
        logF_ (_func, "epoll_create() failed: ", errnoString (errno));
        return Result::Failure;
    }
    logD_ (_this_func, "epoll fd: ", efd);

    if (!posix_createNonblockingPipe (&trigger_pipe)) {
        return Result::Failure;
    }
    logD_ (_this_func, "trigger_pipe fd: read ", trigger_pipe [0], ", write ", trigger_pipe [1]);

    {
        struct epoll_event event;
        event.events =
                  EPOLLET | EPOLLIN
              #ifndef LIBMARY_PLATFORM_ANDROID
                | EPOLLRDHUP
              #endif
                ;
        event.data.u64 = 0; // For valgrind.
        event.data.ptr = NULL; // 'NULL' tells that this is trigger pipe.
        int const res = epoll_ctl (efd, EPOLL_CTL_ADD, trigger_pipe [0], &event);
        if (res == -1) {
            exc_throw (PosixException, errno);
            logF_ (_func, "epoll_ctl() failed: ", errnoString (errno));
            return Result::Failure;
        }

        if (res != 0) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            logF_ (_func, "epoll_ctl(): unexpected return value: ", res);
            return Result::Failure;
        }
    }
  #endif

    return Result::Success;
}

EpollPollGroup::EpollPollGroup (EmbedContainer * const embed_container)
    : ActivePollGroup (embed_container),
      efd (-1),
      triggered (false),
      block_trigger_pipe (true),
      // Initializing to 'true' to process deferred tasks scheduled before we
      // enter poll() for the first time.
      got_deferred_tasks (true)
{
    trigger_pipe [0] = -1;
    trigger_pipe [1] = -1;
}

EpollPollGroup::~EpollPollGroup ()
{
    mutex.lock ();
    {
        PollableList::iterator iter (pollable_list);
        while (!iter.done()) {
            PollableEntry * const pollable_entry = iter.next ();
            delete pollable_entry;
        }
    }

    {
        PollableDeletionQueue::iterator iter (pollable_deletion_queue);
        while (!iter.done()) {
            PollableEntry * const pollable_entry = iter.next ();
            delete pollable_entry;
        }
    }
    mutex.unlock ();

    if (efd != -1)
        posixClose (efd, "efd ");

    for (int i = 0; i < 2; ++i) {
        if (trigger_pipe [i] != -1)
            posixClose (trigger_pipe [i], "trigger_pipe[", i, "] ");
    }
}

}

