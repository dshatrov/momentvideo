/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__EPOLL_POLL_GROUP__H__
#define LIBMARY__EPOLL_POLL_GROUP__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>
#include <libmary/active_poll_group.h>
#include <libmary/state_mutex.h>


namespace M {

class EpollPollGroup : public ActivePollGroup
{
private:
    StateMutex mutex;

    class PollableList_name;
    class PollableDeletionQueue_name;

    class PollableEntry : public IntrusiveListElement<PollableList_name>,
                          public IntrusiveListElement<PollableDeletionQueue_name>
    {
    public:
        mt_const EpollPollGroup *epoll_poll_group;

        mt_const Cb<Pollable> pollable;
        mt_const int fd;

        mt_const bool auto_remove;
        mt_const Object::DeletionSubscriptionKey del_sbn;

        mt_mutex (mutex) bool valid;
    };

    typedef IntrusiveList<PollableEntry, PollableList_name> PollableList;
    typedef IntrusiveList<PollableEntry, PollableDeletionQueue_name> PollableDeletionQueue;

    mt_const int efd;

    // TODO For kqueue, use EVFILT_USER instead
    mt_const int trigger_pipe [2];
    mt_mutex (mutex) bool triggered;
    mt_mutex (mutex) bool block_trigger_pipe;

    mt_sync_domain (poll) bool got_deferred_tasks;

    mt_mutex (mutex) PollableList pollable_list;
    mt_mutex (mutex) PollableDeletionQueue pollable_deletion_queue;

    mt_throws Result doActivate (PollableEntry * mt_nonnull pollable_entry);

    mt_mutex (mutex) void processPollableDeletionQueue ();

    static void pollableDeletionCallback (void *_pollable_entry);

    void removePollable_fromDeletionCb (PollableEntry *pollable_entry);
    void removePollable_fromPoll       (PollableEntry *pollable_entry);
    void doRemovePollable              (PollableEntry *pollable_entry);

public:
  mt_iface (ActivePollgroup)
    mt_iface (PollGroup)
      mt_throws PollableKey addPollable (CbDesc<Pollable> const &pollable_cb,
                                         bool                    auto_remove = false);

      // kqueue is unlike epoll when it comes to delivering events which occured
      // before registering the descriptor. Epoll delivers earlier state changes,
      // and kqueue drops them. That's why we do addPollable in _beforeConnect
      // for kqueue and in _afterConnect for epoll.
      // TODO ^ Doesn't epoll trigger an error when adding an unconnected socket?
      mt_throws Result
        #ifdef LIBMARY_ENABLE_KQUEUE
          addPollable_beforeConnect
        #else
          addPollable_afterConnect
        #endif
                  (CbDesc<Pollable> const &pollable_cb,
                   PollableKey            * const ret_key,
                   bool                     const auto_remove = false)
      {
          PollableKey const key = addPollable (pollable_cb, auto_remove);
          if (!key)
              return Result::Failure;

          if (ret_key)
              *ret_key = key;

          return Result::Success;
      }

      mt_throws Result
        #ifdef LIBMARY_ENABLE_KQUEUE
          addPollable_afterConnect
        #else
          addPollable_beforeConnect
        #endif
                  (CbDesc<Pollable> const & /* pollable_cb */,
                   PollableKey            * const /* ret_key */,
                   bool                     const /* auto_remove */ = false)
          { return Result::Success; }

      void removePollable (PollableKey mt_nonnull key);
    mt_end

    mt_throws Result poll (Uint64 timeout_microsec = (Uint64) -1);

    mt_throws Result trigger ();
  mt_end

    mt_const mt_throws Result open ();

    mt_const void bindToThread (LibMary_ThreadLocal * const /* poll_tlocal */) { /* No-op */ }

     EpollPollGroup (EmbedContainer *embed_container);
    ~EpollPollGroup ();
};

}


#endif /* LIBMARY__EPOLL_POLL_GROUP__H__ */

