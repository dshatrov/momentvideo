/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__POLL_GROUP__H__
#define LIBMARY__POLL_GROUP__H__


#include <libmary/types.h>

#include <libmary/cb.h>
#include <libmary/exception.h>
#include <libmary/deferred_processor.h>

#ifdef LIBMARY_WIN32_IOCP
  #include <libmary/connection.h>
#endif


namespace M {

class PollGroup : public Object
{
public:
    enum EventFlags {
	Input  = 0x1,
	Output = 0x2,
	Error  = 0x4,
	Hup    = 0x8
    };

    // Unused for now. This is meant to be used for multi-threaded WsaPollGroup.
    struct Events {
        void (*pollGroupFull) (void *cb_data);
        void (*pollGroupFree) (void *cb_data);
    };

    class EventSubscriptionKey
    {
    public:
        void *ptr;
        operator bool () const { return ptr; }
        EventSubscriptionKey (void * const ptr = NULL) : ptr (ptr) {}
    };

  #ifndef LIBMARY_WIN32_IOCP
    struct Feedback {
	void (*requestInput)  (void *cb_data);
	void (*requestOutput) (void *cb_data);
    };
  #endif

    struct Pollable
    {
      #ifndef LIBMARY_WIN32_IOCP
	void (*processEvents) (Uint32  event_flags,
			       void   *cb_data);

	void (*setFeedback) (Cb<Feedback> const &feedback,
			     void *cb_data);
      #endif

      #ifdef LIBMARY_PLATFORM_WIN32
	SOCKET (*getFd) (void *cb_data);
      #else
	int (*getFd) (void *cb_data);
      #endif
    };

    class PollableKey
    {
    private:
        void *key;
    public:
      #ifdef LIBMARY_WIN32_IOCP
        // TODO Get rid of activatePollable(), then this field will become unnecessary.
        HANDLE handle;
      #endif
        operator void* () const { return key; }
        PollableKey (void * const key) : key (key) {}
        PollableKey () : key (NULL) {}
    };

public:
    // Every successful call to addPollable() must be matched with a call
    // to removePollable().
    virtual mt_throws PollableKey addPollable (CbDesc<Pollable> const &pollable,
                                               bool                    auto_remove = false) = 0;

    virtual mt_throws Result addPollable_beforeConnect (CbDesc<Pollable> const &pollable,
                                                        PollableKey            *ret_key,
                                                        bool                    auto_remove = false) = 0;

    virtual mt_throws Result addPollable_afterConnect (CbDesc<Pollable> const &pollable,
                                                       PollableKey            *ret_key,
                                                       bool                    auto_remove = false) = 0;

    virtual void removePollable (PollableKey mt_nonnull key) = 0;

    virtual EventSubscriptionKey eventsSubscribe (CbDesc<Events> const & /* cb */)
        { return EventSubscriptionKey (); }

    virtual void eventsUnsubscribe (EventSubscriptionKey /* sbn_key */) {}

    PollGroup (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}

    virtual ~PollGroup () {}
};

}


#include <libmary/active_poll_group.h>


#endif /* LIBMARY__POLL_GROUP__H__ */

