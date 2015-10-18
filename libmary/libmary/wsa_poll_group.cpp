/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>

#include <libmary/wsa_poll_group.h>


namespace M {

static LogGroup libMary_logGroup_wsapoll ("wsapoll", LogLevel::I);

void
WsaPollGroup::informPollGroupFull (Events * const events,
                                   void   * const cb_data,
                                   void   * const /* inform_data */)
{
    if (events->pollGroupFull)
        events->pollGroupFull (cb_data);
}

void
WsaPollGroup::informPollGroupFree (Events * const events,
                                   void   * const cb_data,
                                   void   * const /* inform_data */)
{
    if (events->pollGroupFree)
        events->pollGroupFree (cb_data);
}

void
WsaPollGroup::firePollGroupFull ()
{
    event_informer.informAll (informPollGroupFull, NULL /* inform_data */);
}

void
WsaPollGroup::firePollGroupFree ()
{
    event_informer.informAll (informPollGroupFree, NULL /* inform_data */);
}

mt_mutex (mutex) void
WsaPollGroup::releasePollable (PollableEntry * const mt_nonnull pollable_entry)
{
    if (pollable_entry->wsa_event != WSA_INVALID_EVENT) {
        if (!WSACloseEvent (pollable_entry->wsa_event))
            logE_ (_func, "WSACloseEvent() failed: ", wsaErrorToString (WSAGetLastError()));

        pollable_entry->wsa_event = WSA_INVALID_EVENT;
    }
}

mt_mutex (mutex) void
WsaPollGroup::processPollableDeletionQueue ()
{
    PollableDeletionQueue::iter iter (pollable_deletion_queue);
    while (!pollable_deletion_queue.iter_done (iter)) {
        PollableEntry * const pollable_entry = pollable_deletion_queue.iter_next (iter);
        releasePollable (pollable_entry);
        delete pollable_entry;
    }
    pollable_deletion_queue.clear ();
}

mt_mutex (mutex) mt_throws Result
WsaPollGroup::requestPollableEvents (PollableEntry * const pollable_entry)
{
    long wsa_event_flags = FD_CLOSE;

    if (pollable_entry->need_input)
        wsa_event_flags |= FD_READ | FD_OOB | FD_ACCEPT;

    // Output events.
    wsa_event_flags |= FD_WRITE | FD_CONNECT;

    int const res = WSAEventSelect (pollable_entry->fd, pollable_entry->wsa_event, wsa_event_flags);
    if (res != 0) {
        int const error = WSAGetLastError ();

        if (!WSACloseEvent (pollable_entry->wsa_event)) {
            logE_ (_func, "WSACloseEvent() failed: ", wsaErrorToString (WSAGetLastError()));
        }
        pollable_entry->wsa_event = WSA_INVALID_EVENT;

        if (res != SOCKET_ERROR) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            return Result::Failure;
        }

        exc_throw (WSAException, error);
        return Result::Failure;
    }

    return Result::Success;
}

mt_unlocks (mutex) mt_throws Result
WsaPollGroup::doTrigger ()
{
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

    if (!WSASetEvent (trigger_event)) {
        int const error = WSAGetLastError ();
        logE_ (_func, "WSASetEvent() failed: ", wsaErrorToString (error));
        exc_throw (WSAException, error);
        return Result::Failure;
    }

    return Result::Success;
}

PollGroup::Feedback const WsaPollGroup::pollable_feedback = {
    requestInput,
    requestOutput
};

void
WsaPollGroup::requestInput (void * const _pollable_entry)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> (_pollable_entry);
    // We assume that the poll group is always available when requestInput()
    // is called.
    WsaPollGroup * const self = pollable_entry->wsa_poll_group;

    if (self->poll_tlocal && self->poll_tlocal == libMary_getThreadLocal()) {
        pollable_entry->need_input = true;
    } else {
        self->mutex.lock ();
        pollable_entry->need_input = true;
        mt_unlocks (mutex) self->doTrigger ();
    }
}

void
WsaPollGroup::requestOutput (void * const /* _pollable_entry */)
{
  // No-op. Write notification is edge-triggered.
}

mt_throws PollGroup::PollableKey
WsaPollGroup::addPollable (CbDesc<Pollable> const &pollable)
{
    PollableEntry * const pollable_entry = new (std::nothrow) PollableEntry;
    assert (pollable_entry);

    pollable_entry->wsa_poll_group = this;
    pollable_entry->valid = true;
    pollable_entry->pollable = pollable;
    pollable_entry->fd = pollable->getFd (pollable.cb_data);
    pollable_entry->need_input = true;

    pollable_entry->wsa_event = WSACreateEvent ();
    if (pollable_entry->wsa_event == WSA_INVALID_EVENT) {
        exc_throw (WSAException, WSAGetLastError ());
        releasePollable (pollable_entry);
        delete pollable_entry;
        return NULL;
    }

    {
        if (!requestPollableEvents (pollable_entry)) {
            releasePollable (pollable_entry);
            delete pollable_entry;
            return NULL;
        }
    }

    // We're making an unsafe call, assuming that the pollable is available.
    //
    // We're counting on the fact that the poll group will always be available
    // when pollable_feedback callbacks are called - that's why we use NULL
    // for coderef_container.
    pollable->setFeedback (
            Cb<Feedback> (&pollable_feedback, pollable_entry, NULL /* coderef_container */),
            pollable.cb_data);

    mutex.lock ();
    pollable_list.append (pollable_entry);

    if (!(poll_tlocal && poll_tlocal == libMary_getThreadLocal()))
    {
        if (! mt_unlocks (mutex) doTrigger ())
            logF_ (_func, "doTrigger() failed: ", exc->toString());
    } else {
        mutex.unlock ();
    }

    return static_cast <void*> (pollable_entry);
}

void
WsaPollGroup::removePollable (PollableKey const mt_nonnull key)
{
    PollableEntry * const pollable_entry = static_cast <PollableEntry*> ((void*) key);

    bool delete_now = false;
    mutex.lock ();
    pollable_entry->valid = false;
    pollable_list.remove (pollable_entry);
    pollable_deletion_queue.append (pollable_entry);
    mutex.unlock ();

    if (delete_now) {
        releasePollable (pollable_entry);
        delete pollable_entry;
    }
}

PollGroup::EventSubscriptionKey
WsaPollGroup::eventsSubscribe (CbDesc<Events> const &cb)
{
    GenericInformer::SubscriptionKey const sbn = event_informer.subscribe (cb);
    return sbn.getAsVoidPtr();
}

void
WsaPollGroup::eventsUnsubscribe (EventSubscriptionKey const sbn_key)
{
    GenericInformer::SubscriptionKey const sbn =
            GenericInformer::SubscriptionKey::fromVoidPtr (sbn_key.ptr);
    event_informer.unsubscribe (sbn);
}

mt_throws Result
WsaPollGroup::poll (Uint64 const timeout_microsec)
{
    Time const start_microsec = getTimeMicroseconds ();

    for (;;) {
        logD (wsapoll, _func, "iteration");

        DWORD num_wsa_events = 0;

        {
          // Trigger event should be the first in the list,
          // so that WSAResetEvent() is called only when necessary.
            wsa_events [num_wsa_events] = trigger_event;
            selected_entries [num_wsa_events] = NULL;
            ++num_wsa_events;
        }

        {
            Count i = (Count) num_wsa_events;

            mutex.lock ();
            PollableList::iter iter (pollable_list);
            while (!pollable_list.iter_done (iter)) {
                PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
                if (pollable_entry->wsa_event == WSA_INVALID_EVENT)
                    continue;

                assert (i < WSA_MAXIMUM_WAIT_EVENTS - 1);
                wsa_events [i] = pollable_entry->wsa_event;
                selected_entries [i] = pollable_entry;

                ++i;
            }
            mutex.unlock ();

            num_wsa_events = (DWORD) i;
        }

        logD (wsapoll, _func, "num_wsa_events: ", (unsigned long) num_wsa_events);

        Time const cur_microsec = getTimeMicroseconds ();
        Time elapsed_microsec;
        {
            if (cur_microsec < start_microsec) {
                logE_ (_func, fmt_hex,
                       "cur_microsec: ", cur_microsec, ", "
                       "start_microsec: ", start_microsec);
                unreachable ();
            }
            assert (cur_microsec >= start_microsec);

            elapsed_microsec = cur_microsec - start_microsec;
        }

        Uint64 timeout;
        if (!got_deferred_tasks) {
            if (timeout_microsec != (Uint64) -1) {
                if (timeout_microsec > elapsed_microsec) {
                    timeout = (timeout_microsec - elapsed_microsec) / 1000;
                    if (timeout == 0)
                        timeout = 1;
                } else {
                    timeout = 0;
                }
            } else {
                timeout = (Uint64) -1;
            }
        } else {
            // We've got deferred tasks to process, hence we shouldn't block.
            timeout = 0;
        }

        DWORD dw_timeout = (DWORD) timeout;
        if ((Uint64) dw_timeout != timeout)
            dw_timeout = 0x10000000;

        if (timeout == (Uint64) -1)
            dw_timeout = WSA_INFINITE;

        mutex.lock ();
        if (triggered || dw_timeout == 0) {
            block_trigger_pipe = true;
            dw_timeout = 0;
        } else {
            block_trigger_pipe = false;
        }
        mutex.unlock ();

        DWORD ev_index;
        // Note: I'd rather place do{}while(0) instead of for(;;) to recalculate
        //       dw_timeout before every iteration.
        for (;;) {
            logD (wsapoll, _func, "calling WSAWaitForMultipleEvents(), timeout: ",
                  (unsigned long) dw_timeout);
            DWORD const res = WSAWaitForMultipleEvents (num_wsa_events,
                                                        wsa_events,
                                                        FALSE /* fWaitAll */,
                                                        dw_timeout,
                                                        FALSE /* fAlertable */);
            logD (wsapoll, _func, "res: ", (unsigned long) res);
            if (res == WSA_WAIT_IO_COMPLETION) {
                logD (wsapoll, _func, "WSA_WAIT_IO_COMPLETION");
                continue;
            }

            if (res == WSA_WAIT_TIMEOUT) {
                logD (wsapoll, _func, "WSA_WAIT_TIMEOUT");
                ev_index = num_wsa_events;
                break;
            }

            if (res == WSA_WAIT_FAILED) {
                logD (wsapoll, _func, "WSA_WAIT_FAILED");

                int const error = WSAGetLastError ();
                if (error == WSAEINPROGRESS) {
                    logW_ (_func, "WSAWaitForMultipleEvents(): WSAEINPROGRESS");
                    continue;
                }

                exc_throw (WSAException, error);
                return Result::Failure;
            }

            logD (wsapoll, _func, "WSA_WAIT_EVENT_0: ",
                  (unsigned long) WSA_WAIT_EVENT_0,
                  ", sum: ", (unsigned long) (WSA_WAIT_EVENT_0 + num_wsa_events));
            if (! (res >= WSA_WAIT_EVENT_0 && res < WSA_WAIT_EVENT_0 + num_wsa_events)) {
                logD (wsapoll, _func, "unexpected return value: ", (unsigned long) res);

                exc_throw (InternalException, InternalException::BackendMalfunction);
                return Result::Failure;
            }

            ev_index = res - WSA_WAIT_EVENT_0;
            logD (wsapoll, _func, "ev_index: ", (unsigned long) ev_index);
            break;
        }
        assert (ev_index <= num_wsa_events);

        mutex.lock ();
        block_trigger_pipe = true;
        bool const was_triggered = triggered;
        triggered = false;
        mutex.unlock ();

        got_deferred_tasks = false;

        if (frontend)
            frontend.call (frontend->pollIterationBegin);

        bool trigger_event_ready = false;

        mutex.lock ();
        for (unsigned i = ev_index; i < num_wsa_events; ++i) {
            PollableEntry * const pollable_entry = selected_entries [i];

            if (pollable_entry == NULL) {
              // Trigger event.
                logD (wsapoll, _func, "trigger event ready");
                trigger_event_ready = true;
                continue;
            }

            if (pollable_entry->valid) {
                logD (wsapoll, _func, "processing pollable");

                WSANETWORKEVENTS net_events;
                {
                    int const res = WSAEnumNetworkEvents (pollable_entry->fd,
                                                          wsa_events [i],
                                                          &net_events);
                    if (res == SOCKET_ERROR) {
                        int const error = WSAGetLastError ();
                        mutex.unlock ();
                        logE_ (_func, "WSAEnumNetworkEvents() failed: ", wsaErrorToString (error));
                        exc_throw (WSAException, error);
                        return Result::Failure;
                    } else
                    if (res != 0) {
                        mutex.unlock ();
                        logE_ (_func, "WSAEnumNetworkEvents(): unexpected return value");
                        exc_throw (InternalException, InternalException::BackendMalfunction);
                        return Result::Failure;
                    }
                }

                bool const prv_need_input  = pollable_entry->need_input;

                Uint32 event_flags = 0;

                if (net_events.lNetworkEvents & FD_READ ||
                    net_events.lNetworkEvents & FD_OOB  ||
                    net_events.lNetworkEvents & FD_ACCEPT)
                {
                    logD (wsapoll, _func, "input event");

                    pollable_entry->need_input = false;
                    event_flags |= PollGroup::Input;
                }

                if (net_events.lNetworkEvents & FD_WRITE ||
                    net_events.lNetworkEvents & FD_CONNECT)
                {
                    logD (wsapoll, _func, "output event");

                    event_flags |= PollGroup::Output;
                }

                if (net_events.lNetworkEvents & FD_CLOSE) {
                    logD (wsapoll, _func, "hup event");

                    event_flags |= PollGroup::Hup;
                }

                if (event_flags) {
                    mt_unlocks_locks (mutex) pollable_entry->pollable.call_mutex (
                            pollable_entry->pollable->processEvents,
                            mutex,
                            /*(*/ event_flags /*)*/);
                }

                if (pollable_entry->need_input != prv_need_input) {
                    logD (wsapoll, _func, "calling requestPollableEvents(), "
                          "need_input: ", pollable_entry->need_input);
                    if (!requestPollableEvents (pollable_entry)) {
                        logE_ (_func, "requestPollableEvents() failed: ", exc->toString());

                        mt_unlocks_locks (mutex) pollable_entry->pollable.call_mutex (
                                pollable_entry->pollable->processEvents,
                                mutex,
                                /*(*/ PollGroup::Error /*)*/);

                        mutex.unlock ();
                        return Result::Failure;
                    }
                }
            }
        }

        processPollableDeletionQueue ();
        mutex.unlock ();

        if (frontend) {
            bool extra_iteration_needed = false;
            frontend.call_ret (&extra_iteration_needed, frontend->pollIterationEnd);
            if (extra_iteration_needed)
                got_deferred_tasks = true;
        }

        if (trigger_event_ready) {
            for (;;) {
                logD (wsapoll, _func, "resetting trigger event");

                // TODO Use auto-resetting event for trigger_event.
                BOOL const res = WSAResetEvent (trigger_event);
                if (!res) {
                    int const error = WSAGetLastError ();
                    if (error == WSAEINPROGRESS) {
                        logW_ (_func, "WSAResetEvent(): WSAEINPROGRESS");
                        continue;
                    }

                    logE_ (_func, "WSAResetEvent() failed: ", wsaErrorToString (error));
                    exc_throw (WSAException, error);
                    return Result::Failure;
                }

                break;
            }
        }

        if (was_triggered) {
            logD (wsapoll, _func, "trigger_break");
            break;
        }

        if (elapsed_microsec >= timeout_microsec) {
          // Timeout expired.
            logD (wsapoll, _func, "timeout expired");
            break;
        }
    } // for (;;)

    logD (wsapoll, _func, "returning");
    return Result::Success;
}

mt_throws Result
WsaPollGroup::trigger ()
{
    if (poll_tlocal && poll_tlocal == libMary_getThreadLocal()) {
        mutex.lock ();
        triggered = true;
        mutex.unlock ();
        return Result::Success;
    }

    mutex.lock ();
    return mt_unlocks (mutex) doTrigger ();
}

mt_const mt_throws Result
WsaPollGroup::open ()
{
    trigger_event = WSACreateEvent ();
    if (trigger_event == WSA_INVALID_EVENT) {
        int const err = WSAGetLastError();
        logF_ (_func, "WSACreateEvent() failed: ", wsaErrorToString (err));
        exc_throw (WSAException, err);
        return Result::Failure;
    }

    return Result::Success;
}

WsaPollGroup::WsaPollGroup (Object * const coderef_container)
    : DependentCodeReferenced (coderef_container),
      poll_tlocal        (NULL),
      trigger_event      (WSA_INVALID_EVENT),
      triggered          (false),
      block_trigger_pipe (true),
      wsa_events         (WSA_MAXIMUM_WAIT_EVENTS),
      selected_entries   (WSA_MAXIMUM_WAIT_EVENTS),
      got_deferred_tasks (true),
      event_informer     (coderef_container, &mutex)
{
    trigger_event = WSA_INVALID_EVENT;
}

WsaPollGroup::~WsaPollGroup ()
{
    mutex.lock ();
    {
        PollableList::iter iter (pollable_list);
        while (!pollable_list.iter_done (iter)) {
            PollableEntry * const pollable_entry = pollable_list.iter_next (iter);
            releasePollable (pollable_entry);
            delete pollable_entry;
        }
    }

    {
        PollableDeletionQueue::iter iter (pollable_deletion_queue);
        while (!pollable_deletion_queue.iter_done (iter)) {
            PollableEntry * const pollable_entry = pollable_deletion_queue.iter_next (iter);
            releasePollable (pollable_entry);
            delete pollable_entry;
        }
    }
    mutex.unlock ();

    if (trigger_event != WSA_INVALID_EVENT) {
        if (!WSACloseEvent (trigger_event))
            logE_ (_func, "WSACloseEvent() failed: ", wsaErrorToString (WSAGetLastError()));
    }
}

}

