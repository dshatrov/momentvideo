/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__TCP_SERVER__LINUX__H__
#define LIBMARY__TCP_SERVER__LINUX__H__


#include <libmary/exception.h>
#include <libmary/poll_group.h>
#include <libmary/timers.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/tcp_connection.linux.h>
#else
  #include <libmary/tcp_connection.h>
#endif


namespace M {

class TcpServer : public Object
{
public:
    struct Frontend
    {
	void (*accepted) (void *cb_data);
    };

private:
    mt_const Time accept_retry_timeout_millisec;

    mt_const Ref<Timers> timers;

    int fd;

    Cb<Frontend> frontend;
    Cb<PollGroup::Feedback> feedback;

    void requestInput ()
    {
	if (feedback && feedback->requestInput)
	    feedback.call (feedback->requestInput);
    }

    void requestOutput ()
    {
	if (feedback && feedback->requestOutput)
	    feedback.call (feedback->requestOutput);
    }

  mt_iface (PollGroup::Pollable)
    static PollGroup::Pollable const pollable;

    static void processEvents (Uint32  event_flags,
			       void   *_self);

    static int getFd (void *_self);

    static void setFeedback (Cb<PollGroup::Feedback> const &feedback,
			     void *_self);
  mt_iface_end

    StateMutex accept_retry_mutex;
    mt_mutex (mutex) bool accept_retry_timer_set;

    static void acceptRetryTimerTick (void *_data);

public:
    mt_throws Result open ();

    enum_beg (AcceptResult)
        enum_values (Accepted, NotAccepted, Error)
    enum_end (AcceptResult)

    mt_throws AcceptResult accept (TcpConnection * mt_nonnull tcp_connection,
				   IpAddress     *ret_addr = NULL);

    // Should be called before listen().
    mt_throws Result bind (IpAddress ip_addr);

    // Should only be called once.
    mt_throws Result listen ();

    mt_throws Result start ();

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    void init (CbDesc<Frontend> const &frontend,
               DeferredProcessor * mt_nonnull deferred_processor,
               Timers            * mt_nonnull timers,
               Time               accept_retry_timeout_millisec = 1000);

     TcpServer (EmbedContainer *embed_container);
    ~TcpServer ();
};

}


#endif /* LIBMARY__TCP_SERVER__LINUX__H__ */

