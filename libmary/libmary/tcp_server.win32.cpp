/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/common_win32.h>
#include <libmary/log.h>

#include <libmary/tcp_server.h>


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
	logE_ (_func, "PollGroup::Hup");

    if (event_flags & PollGroup::Output)
	logE_ (_func, "PollGroup::Output");

    if (event_flags & PollGroup::Input) {
	if (self->frontend && self->frontend->accepted)
	    self->frontend.call (self->frontend->accepted);
    }

    if (event_flags & PollGroup::Error)
	logE_ (_func, "PollGroup::Error");
}

void
TcpServer::setFeedback (Cb<PollGroup::Feedback> const &feedback,
                        void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    self->feedback = feedback;
}

SOCKET
TcpServer::getFd (void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    return self->fd;
}

mt_throws Result
TcpServer::open ()
{
    fd = WSASocket (AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (fd == INVALID_SOCKET) {
        exc_throw (WSAException, WSAGetLastError ());
        exc_throw (InternalException, InternalException::BackendError);
        return Result::Failure;
    }

    {
        u_long value = 1;
        int const res = ioctlsocket (fd, FIONBIO, &value);
        if (res != 0) {
            if (res != SOCKET_ERROR) {
                exc_throw (InternalException, InternalException::BackendMalfunction);
                logE_ (_func, "ioctlsocket(): unexpected return value: ", res);
                return Result::Failure;
            }

            int const error = WSAGetLastError ();
            exc_throw (WSAException, error);
            exc_push (InternalException, InternalException::BackendError);
            logE_ (_func, "ioctlsocket() failed: ", wsaErrorToString (error));
            return Result::Failure;
        }
    }

    if (!_libMary_win32_setsockopt_reuseaddr (fd))
        return Result::Failure;

    return Result::Success;
}

// TODO Handle accept errors in a way similar to tcp_server.linux.cpp
//
mt_throws TcpServer::AcceptResult
TcpServer::accept (TcpConnection * const mt_nonnull tcp_connection,
                   IpAddress     * const ret_addr)
{
    if (ret_addr)
        ret_addr->reset ();

    SOCKET conn_fd;
    for (;;) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof (client_addr);

        conn_fd = ::accept (fd, (struct sockaddr*) &client_addr, &client_addr_len);
        if (conn_fd == INVALID_SOCKET) {
            int const error = WSAGetLastError ();

            if (error == WSAECONNRESET ||
                error == WSAEINTR)
            {
                continue;
            }

            if (error == WSAEWOULDBLOCK) {
                requestInput ();
                return AcceptResult::NotAccepted;
            }

            // NOTE: We could handle WSAEMFILE and WSAENOBUFS here (retry timer).

            exc_throw (WSAException, error);
            exc_push (InternalException, InternalException::BackendError);

            logE_ (_func, "accept() failed: ", wsaErrorToString (error));

            return AcceptResult::Error;
        }

        break;
    }

    {
        u_long value = 1;
        int const res = ioctlsocket (conn_fd, FIONBIO, &value);
        if (res != 0) {
            if (res != SOCKET_ERROR) {
                exc_throw (InternalException, InternalException::BackendMalfunction);
                logE_ (_func, "ioctlsocket(): unexpected return value: ", res);
                goto _failure;
            }

            int const error = WSAGetLastError ();
            exc_throw (WSAException, error);
            exc_push (InternalException, InternalException::BackendError);
            logE_ (_func, "ioctlsocket() failed: ", wsaErrorToString (error));
            goto _failure;
        }
    }

    {
	int opt_val = 1;
	int const res = setsockopt (conn_fd, IPPROTO_TCP, TCP_NODELAY, (char*) &opt_val, sizeof (opt_val));
	if (res == SOCKET_ERROR) {
            int const error = WSAGetLastError ();
	    exc_throw (WSAException, error);
	    exc_push (InternalException, InternalException::BackendError);
	    logE_ (_func, "setsockopt() failed (TCP_NODELAY): ", wsaErrorToString (error));
            goto _failure;
	} else
	if (res != 0) {
	    exc_throw (InternalException, InternalException::BackendMalfunction);
	    logE_ (_func, "setsockopt() (TCP_NODELAY): unexpected return value: ", res);
            goto _failure;
	}
    }

    tcp_connection->setFd (conn_fd);

    return AcceptResult::Accepted;

_failure:
    {
        int const res = ::closesocket (conn_fd);
        if (res == SOCKET_ERROR) {
            int const error = WSAGetLastError ();
            exc_throw (WSAException, error);
            logE_ (_func, "close() failed: ", wsaErrorToString (error));
        } else
        if (res != 0) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            logE_ (_func, "close(): unexpected return value: ", res);
        }
    }

    return AcceptResult::Error;
}

mt_throws Result
TcpServer::bind (IpAddress const ip_addr)
    { return _libMary_win32_bind (fd, ip_addr); }

mt_throws Result
TcpServer::listen ()
    { return _libMary_win32_listen (fd); }

mt_throws Result
TcpServer::start ()
    { return Result::Success; }

void
TcpServer::init (CbDesc<Frontend> const &frontend,
                 DeferredProcessor * const mt_nonnull /* deferred_processor */,
                 Timers            * const mt_nonnull timers,
                 Time                const accept_retry_timeout_millisec)
{
    this->frontend = frontend;
}

TcpServer::~TcpServer ()
{
    _libMary_win32_closesocket (fd);
}

}

