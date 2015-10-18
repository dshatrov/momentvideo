/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ACTIVE_POLL_GROUP__H__
#define LIBMARY__ACTIVE_POLL_GROUP__H__


#include <libmary/poll_group.h>


namespace M {

class ActivePollGroup : public PollGroup
{
public:
    struct Frontend {
	// pollIterationBegin is not called when poll() returns (poll timeout/error).
	void (*pollIterationBegin) (void *cb_data);

	// If returns 'true', then there's more work to do in pollIterationEnd(),
	// and the next poll iteration will be performed with zero timeout.
	bool (*pollIterationEnd)   (void *cb_data);
    };

protected:
    mt_const Cb<Frontend> frontend;

public:
    virtual mt_throws Result poll (Uint64 timeout_millisec = (Uint64) -1) = 0;

    virtual mt_throws Result trigger () = 0;

    void setFrontend (Cb<Frontend> const &frontend)
        { this->frontend = frontend; }

    ActivePollGroup (EmbedContainer * const embed_container)
        : PollGroup (embed_container)
    {}

    virtual ~ActivePollGroup () {}
};


#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_IOCP
    class IocpPollGroup;
    typedef IocpPollGroup DefaultPollGroup;
  #else
    class WsaPollGroup;
    typedef WsaPollGroup DefaultPollGroup;
  #endif
#else
  #if defined (LIBMARY_USE_SELECT)
    class SelectPollGroup;
    typedef SelectPollGroup DefaultPollGroup;
  #elif defined (LIBMARY_USE_POLL) || (!defined (LIBMARY_ENABLE_EPOLL) && !defined (LIBMARY_ENABLE_KQUEUE))
    class PollPollGroup;
    typedef PollPollGroup DefaultPollGroup;
  #else
    #if defined (LIBMARY_ENABLE_EPOLL) || defined (LIBMARY_ENABLE_KQUEUE)
      class EpollPollGroup;
      typedef EpollPollGroup DefaultPollGroup;
    #endif
  #endif
#endif
}


#ifdef LIBMARY_PLATFORM_WIN32
  #ifdef LIBMARY_WIN32_IOCP
    #include <libmary/iocp_poll_group.h>
  #else
    #include <libmary/wsa_poll_group.h>
  #endif
#else
  /* Commented out because of missing auto_remove support.
  #include <libmary/select_poll_group.h>
  #include <libmary/poll_poll_group.h>
  */
  #if defined (LIBMARY_ENABLE_EPOLL) || defined (LIBMARY_ENABLE_KQUEUE)
    #include <libmary/epoll_poll_group.h>
  #endif
#endif


#endif /* LIBMARY__ACTIVE_POLL_GROUP__H__ */

