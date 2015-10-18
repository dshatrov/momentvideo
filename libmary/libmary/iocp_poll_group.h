/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__IOCP_POLL_GROUP__H__
#define LIBMARY__IOCP_POLL_GROUP__H__


#include <libmary/active_poll_group.h>


namespace M {

// Note that WSAOVERLAPPED should be used for WSA* functions,
// but msdn says it's compatible with OVERLAPPED.
//
struct Overlapped : public OVERLAPPED, public Referenced
{
    typedef void IoCompleteCallback (Exception *exc_,
                                     Overlapped *overlapped,
                                     Size        bytes_transferred,
                                     void       *cb_data);

    Cb<IoCompleteCallback> io_complete_cb;
};

class IocpPollGroup : public ActivePollGroup
{
private:
    StateMutex mutex;

    mt_const HANDLE iocp_handle;

    mt_mutex (mutex) bool triggered;
    mt_mutex (mutex) bool block_trigger_pipe;

    mt_sync_domain (poll) bool got_deferred_tasks;

    mt_throws Result doActivate (HANDLE handle);

public:
  mt_iface (ActivePollGroup)
    mt_iface (PollGroup)
      mt_throws PollableKey addPollable (CbDesc<Pollable> const &pollable,
                                         bool                    auto_remove = false);

      mt_throws Result addPollable_beforeConnect (CbDesc<Pollable> const &pollable,
                                                  PollableKey            * const ret_key,
                                                  bool                     const auto_remove = false)
      {
          PollableKey const key = addPollable (pollable, auto_remove);
          if (!key)
              return Result::Failure;

          if (ret_key)
              *ret_key = key;

          return Result::Success;
      }

      mt_throws Result addPollable_afterConnect (CbDesc<Pollable> const & /* pollable */,
                                                 PollableKey            * const /* ret_key */,
                                                 bool                     const /* auto_remove */ = false)
          { return Result::Success; }

      void removePollable (PollableKey mt_nonnull key);
    mt_iface_end

    mt_throws Result poll (Uint64 timeout_microsec = (Uint64) -1);

    mt_throws Result trigger ();
  mt_iface_end

    mt_const mt_throws Result open ();

    mt_const void bindToThread (LibMary_ThreadLocal * const /* poll_tlocal */) { /* No-op */ }

     IocpPollGroup (EmbedContainer *embed_container);
    ~IocpPollGroup ();
};

}


#endif /* LIBMARY__IOCP_POLL_GROUP__H__ */

