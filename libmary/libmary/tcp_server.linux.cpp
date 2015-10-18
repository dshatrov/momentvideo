/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <libmary/log.h>
#include <libmary/util_net.h>
#include <libmary/util_posix.h>

#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/tcp_server.linux.h>
#else
  #include <libmary/tcp_server.h>
#endif

namespace M {

PollGroup::Pollable const TcpServer::pollable = {
    processEvents,
    setFeedback,
    getFd
};

void
TcpServer::processEvents (Uint32   const event_flags,
			  void   * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);

    if (event_flags & PollGroup::Hup)
	logE_ (_self_func, "PollGroup::Hup");

    if (event_flags & PollGroup::Output)
	logE_ (_self_func, "PollGroup::Output");

    if (event_flags & PollGroup::Input) {
	if (self->frontend)
	    self->frontend.call (self->frontend->accepted);
    }

    if (event_flags & PollGroup::Error)
	logE_ (_self_func, "PollGroup::Error");
}

int
TcpServer::getFd (void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    return self->fd;
}

void
TcpServer::setFeedback (Cb<PollGroup::Feedback> const &feedback,
			void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    self->feedback = feedback;
}

void
TcpServer::acceptRetryTimerTick (void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);

    self->accept_retry_mutex.lock ();
    self->accept_retry_timer_set = false;
    self->accept_retry_mutex.unlock ();

    if (self->frontend)
        self->frontend.call (self->frontend->accepted);
}

mt_throws Result
TcpServer::open ()
{
    fd = socket (AF_INET, SOCK_STREAM, /*protocol=*/ 0);
    if (fd == -1) {
	exc_throw (PosixException, errno);
	exc_push (InternalException, InternalException::BackendError);
	logE_ (_this_func, "socket() failed: ", errnoString (errno));
	return Result::Failure;
    }

    {
	int flags = fcntl (fd, F_GETFL, 0);
	if (flags == -1) {
	    exc_throw (PosixException, errno);
	    exc_push (InternalException, InternalException::BackendError);
	    logE_ (_this_func, "fcntl() failed (F_GETFL): ", errnoString (errno));
	    return Result::Failure;
	}

	flags |= O_NONBLOCK;

	if (fcntl (fd, F_SETFL, flags) == -1) {
	    exc_throw (PosixException, errno);
	    exc_push (InternalException, InternalException::BackendError);
	    logE_ (_this_func, "fcntl() failed (F_SETFL): ", errnoString (errno));
	    return Result::Failure;
	}
    }

    {
	int opt_val = 1;
	int const res = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof (opt_val));
	if (res == -1) {
	    exc_throw (PosixException, errno);
	    exc_push (InternalException, InternalException::BackendError);
	    logE_ (_this_func, "setsockopt() failed (SO_REUSEADDR): ", errnoString (errno));
	    return Result::Failure;
	} else
	if (res != 0) {
	    exc_throw (InternalException, InternalException::BackendMalfunction);
	    logE_ (_this_func, "setsockopt (SO_REUSEADDR): unexpected return value: ", res);
	    return Result::Failure;
	}
    }

    return Result::Success;
}

mt_throws TcpServer::AcceptResult
TcpServer::accept (TcpConnection * const mt_nonnull tcp_connection,
		   IpAddress     * const ret_addr)
{
    int conn_fd;
    for (;;) {
        conn_fd = -1;
        if (ret_addr)
            ret_addr->reset ();

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof (client_addr);

	conn_fd = ::accept (fd, (struct sockaddr*) &client_addr, &client_addr_len);
	if (conn_fd == -1) {
	    if (   errno == EINTR /* || errno == ERESTARTSYS */
		|| errno == EPROTO
		|| errno == ECONNABORTED
                || errno == EPERM)
	    {
	      // Note that we shouldn't return until we're sure that there's
	      // no pending client connections on the server socket (this method
	      // can be thought of as it was named "acceptFull" to reflect this).
	      // Otherwise we would loose level-triggered socket state.
                logW_ (_this_func, "accept(): ", errnoString (errno));
		continue;
	    }

	    if (errno == EAGAIN || errno == EWOULDBLOCK) {
		requestInput ();
		return AcceptResult::NotAccepted;
	    }

            if (   errno == EMFILE
                || errno == ENFILE
                || errno == ENOBUFS
                || errno == ENOMEM)
            {
                ConstMemory err_str = "Unknown";
                switch (errno) {
                    case EMFILE:  err_str = "EMFILE"; break;
                    case ENFILE:  err_str = "ENFILE"; break;
                    case ENOBUFS: err_str = "ENOBUFS"; break;
                    case ENOMEM:  err_str = "ENOMEM"; break;
                }
                logE_ (_this_func, "accept() failed: ", err_str, ": ", errnoString (errno));
            } else {
                logE_ (_this_func, "accept() failed: ", errnoString (errno));
            }

            // For EMFILE and friends, we just wait for better times, which is 
            // the right thing to do.
            // For other (unknown) errors, we wait just for extra safety,
            // to avoid busy-looping. We don't stop the server on accept() errors
            // because there's no fixed list of errors for accept() to rely on
            // for strict semantics.
            accept_retry_mutex.lock ();
            if (!accept_retry_timer_set) {
                accept_retry_timer_set = true;
                timers->addTimer_microseconds (
                        CbDesc<Timers::TimerCallback> (acceptRetryTimerTick, this, this),
                        accept_retry_timeout_millisec * 1000,
                        false /* periodical */,
                        true  /* auto_delete */,
                        true  /* delete_after_tick */);
            }
            accept_retry_mutex.unlock ();

            return AcceptResult::NotAccepted;
	}

	if (ret_addr)
	    setIpAddress (&client_addr, ret_addr);

        {
            int flags = fcntl (conn_fd, F_GETFL, 0);
            if (flags == -1) {
                exc_throw (PosixException, errno);
                exc_push (InternalException, InternalException::BackendError);
                logE_ (_this_func, "fcntl() failed (F_GETFL): ", errnoString (errno));
                goto _failure;
            }

            flags |= O_NONBLOCK;

            if (fcntl (conn_fd, F_SETFL, flags) == -1) {
                exc_throw (PosixException, errno);
                exc_push (InternalException, InternalException::BackendError);
                logE_ (_this_func, "fcntl() failed (F_SETFL): ", errnoString (errno));
                goto _failure;
            }
        }

        {
            int opt_val = 1;
            int const res = setsockopt (conn_fd, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof (opt_val));
            if (res == -1) {
                exc_throw (PosixException, errno);
                exc_push (InternalException, InternalException::BackendError);
                logE_ (_this_func, "setsockopt() failed (TCP_NODELAY): ", errnoString (errno));
                goto _failure;
            } else
            if (res != 0) {
                exc_throw (InternalException, InternalException::BackendMalfunction);
                logE_ (_this_func, "setsockopt() (TCP_NODELAY): unexpected return value: ", res);
                goto _failure;
            }
        }

      #ifdef __linux__
        {
            int opt_val = 1;
            int const res = setsockopt (conn_fd, IPPROTO_TCP, TCP_QUICKACK, &opt_val, sizeof (opt_val));
            if (res == -1) {
                exc_throw (PosixException, errno);
                exc_push (InternalException, InternalException::BackendError);
                logE_ (_this_func, "setsockopt() failed (TCP_QUICKACK): ", errnoString (errno));
                goto _failure;
            } else
            if (res != 0) {
                exc_throw (InternalException, InternalException::BackendMalfunction);
                logE_ (_this_func, "setsockopt() (TCP_QUICKACK): unexpected return value: ", res);
                goto _failure;
            }
        }
      #endif /* __linux__ */

        tcp_connection->setFd (conn_fd);
        return AcceptResult::Accepted;

_failure:
        posixClose (conn_fd, _this_func);

      // Eating all errors to avoid missing incoming connection events,
      // which would lead to not accepting any more connections (DoS).
    } /* for (;;) */

    unreachable ();
}

mt_throws Result
TcpServer::bind (IpAddress const ip_addr)
{
    struct sockaddr_in addr;
    setIpAddress (ip_addr, &addr);

    int const res = ::bind (fd, (struct sockaddr*) &addr, sizeof (addr));
    if (res == -1) {
	exc_throw (PosixException, errno);
	exc_push (InternalException, InternalException::BackendError);
	logE_ (_this_func, "bind() failed: ", errnoString (errno));
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	logE_ (_this_func, "bind(): unexpected return value: ", res);
	return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
TcpServer::listen ()
{
    int const res = ::listen (fd, SOMAXCONN);
    if (res == -1) {
	exc_throw (PosixException, errno);
	exc_push (InternalException, InternalException::BackendError);
	logE_ (_this_func, "listen() failed: ", errnoString (errno));
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	logE_ (_this_func, "listen(): unexpected return value: ", res);
	return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
TcpServer::start ()
    { return Result::Success; }

void
TcpServer::init (CbDesc<Frontend> const &frontend,
                 DeferredProcessor * const mt_nonnull /* deferred_processor */,
                 Timers            * const mt_nonnull timers,
                 Time                const accept_retry_timeout_millisec)
{
    assert (timers);

    this->frontend = frontend;
    this->timers = timers;
    this->accept_retry_timeout_millisec = accept_retry_timeout_millisec;
}

TcpServer::TcpServer (EmbedContainer * const embed_container)
    : Object (embed_container),
      accept_retry_timeout_millisec (1000),
      fd (-1),
      accept_retry_timer_set (false)
{
}

TcpServer::~TcpServer ()
{
    posixClose (fd, _this_func);
}

}

