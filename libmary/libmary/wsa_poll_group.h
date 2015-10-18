/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__WSA_POLL_GROUP__H__
#define LIBMARY__WSA_POLL_GROUP__H__


#include <libmary/types.h>

#include <libmary/code_referenced.h>
#include <libmary/intrusive_list.h>
#include <libmary/array_holder.h>
#include <libmary/informer.h>
#include <libmary/active_poll_group.h>
#include <libmary/state_mutex.h>


namespace M {

class WsaPollGroup : public ActivePollGroup,
                     public DependentCodeReferenced
{
private:
    StateMutex mutex;

    class PollableList_name;
    class PollableDeletionQueue_name;

    class PollableEntry : public IntrusiveListElement<PollableList_name>,
                          public IntrusiveListElement<PollableDeletionQueue_name>
    {
    public:
        mt_const WsaPollGroup *wsa_poll_group;
        mt_const WSAEVENT wsa_event;

        mt_const Cb<Pollable> pollable;
        mt_const SOCKET fd;

        mt_mutex (mutex) bool valid;

        mt_mutex (mutex) bool need_input;
    };

    typedef IntrusiveList< PollableEntry, PollableList_name > PollableList;
    typedef IntrusiveList< PollableEntry, PollableDeletionQueue_name> PollableDeletionQueue;

    mt_const LibMary_ThreadLocal *poll_tlocal;

    mt_const WSAEVENT trigger_event;
    mt_mutex (mutex) bool triggered;
    mt_mutex (mutex) bool block_trigger_pipe;

    mt_mutex (mutex) PollableList pollable_list;
    mt_mutex (mutex) PollableDeletionQueue pollable_deletion_queue;

    mt_sync_domain (poll) ArrayHolder<WSAEVENT> wsa_events;
    mt_sync_domain (poll) ArrayHolder<PollableEntry*> selected_entries;
    mt_sync_domain (poll) bool got_deferred_tasks;

    Informer_<Events> event_informer;

    static void informPollGroupFull (Events *events,
                                     void   *cb_data,
                                     void   *inform_data);

    static void informPollGroupFree (Events *events,
                                     void   *cb_data,
                                     void   *inform_data);

    void firePollGroupFull ();
    void firePollGroupFree ();

    mt_mutex (mutex) void releasePollable (PollableEntry * mt_nonnull pollable_entry);
    mt_mutex (mutex) void processPollableDeletionQueue ();

    mt_mutex (mutex) mt_throws Result requestPollableEvents (PollableEntry *pollable_entry);

    mt_unlocks (mutex) mt_throws Result doTrigger ();

  mt_iface (PollGroup::Feedback)
    static Feedback const pollable_feedback;
    static void requestInput  (void *_pollable_entry);
    static void requestOutput (void *_pollable_entry);
  mt_iface_end

public:
  mt_iface (ActivePollGroup)
    mt_iface (PollGroup)
      mt_throws PollableKey addPollable (CbDesc<Pollable> const &pollable);

      mt_throws Result addPollable_beforeConnect (CbDesc<Pollable> const & /* pollable */,
                                                  PollableKey * const /* ret_key */)
          { return Result::Success; }

      mt_throws Result addPollable_afterConnect (CbDesc<Pollable> const &pollable,
                                                 PollableKey * const ret_key)
      {
          PollableKey const key = addPollable (pollable);
          if (!key)
              return Result::Failure;

          if (ret_key)
              *ret_key = key;

          return Result::Success;
      }

      void removePollable (PollableKey mt_nonnull key);

      virtual EventSubscriptionKey eventsSubscribe (CbDesc<Events> const &cb);

      virtual void eventsUnsubscribe (EventSubscriptionKey sbn_key);
    mt_iface_end

        // Must be called from the same thread every time.
    mt_throws Result poll (Uint64 timeout_microsec = (Uint64) -1);

    mt_throws Result trigger ();
  mt_iface_end

    mt_const mt_throws Result open ();

    mt_const void bindToThread (LibMary_ThreadLocal * const poll_tlocal)
        { this->poll_tlocal = poll_tlocal; }

     WsaPollGroup (Object *coderef_container);
    ~WsaPollGroup ();
};

}


#endif /* LIBMARY__WSA_POLL_GROUP__H__ */

