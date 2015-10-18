/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/util_str.h>
#include <libmary/log.h>

#include <libmary/common_win32.h>


namespace M {

mt_throws Result _libMary_win32_socket_tcp (SOCKET * const mt_nonnull ret_socket_fd,
                                            bool     const overlapped)
{
    *ret_socket_fd = WSASocket (AF_INET,
                                SOCK_STREAM,
                                IPPROTO_TCP,
                                NULL /* lpProtocolInfo */,
                                0    /* group */,
                                (overlapped ? WSA_FLAG_OVERLAPPED : 0));
    if (*ret_socket_fd == INVALID_SOCKET) {
        int const err = WSAGetLastError();
	logF_ (_func, "WSASocket() failed: ", wsaErrorToString (err));
	exc_throw (WSAException, err);
        return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result _libMary_win32_setsockopt_nodelay (SOCKET const socket_fd)
{
    int opt_val = 1;
    int const res = setsockopt (socket_fd, IPPROTO_TCP, TCP_NODELAY, (char*) &opt_val, sizeof (opt_val));
    if (res == SOCKET_ERROR) {
        int const err = WSAGetLastError();
        logE_ (_func, "setsockopt() failed (TCP_NODELAY): ", wsaErrorToString (err));
        exc_throw (WSAException, err);
        return Result::Failure;
    } else
    if (res != 0) {
        logE_ (_func, "setsockopt() (TCP_NODELAY): unexpected return value: ", res);
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result _libMary_win32_setsockopt_reuseaddr (SOCKET const socket_fd)
{
    int opt_val = 1;
    int const res = setsockopt (socket_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &opt_val, sizeof (opt_val));
    if (res == SOCKET_ERROR) {
        int const err = WSAGetLastError();
        logE_ (_func, "setsockopt() failed (SO_REUSEADDR): ", wsaErrorToString (err));
        exc_throw (WSAException, err);
        return Result::Failure;
    } else
    if (res != 0) {
        logE_ (_func, "setsockopt() (SO_REUSEADDR): unexpected return value: ", res);
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result _libMary_win32_bind (SOCKET    const socket_fd,
                                      IpAddress const ip_addr)
{
    struct sockaddr_in addr;
    setIpAddress (ip_addr, &addr);

    int const res = ::bind (socket_fd, (struct sockaddr*) &addr, sizeof (addr));
    if (res == SOCKET_ERROR) {
        int const err = WSAGetLastError();
	logE_ (_func, "bind() failed: ", wsaErrorToString (err));
	exc_throw (WSAException, err);
	return Result::Failure;
    } else
    if (res != 0) {
	logE_ (_func, "bind(): unexpected return value: ", res);
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result _libMary_win32_listen (SOCKET const socket_fd)
{
    int const res = ::listen (socket_fd, SOMAXCONN);
    if (res == SOCKET_ERROR) {
        int const err = WSAGetLastError();
	logF_ (_func, "listen() failed: ", wsaErrorToString (err));
	exc_throw (WSAException, err);
	return Result::Failure;
    } else
    if (res != 0) {
	logF_ (_func, "listen(): unexpected return value: ", res);
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result _libMary_win32_closesocket (SOCKET const socket_fd)
{
    if (socket_fd != INVALID_SOCKET) {
        int const res = ::closesocket (socket_fd);
        if (res == SOCKET_ERROR) {
            int const err = WSAGetLastError();
            logE_ (_func, "closesocket() failed: ", wsaErrorToString (err));
            exc_throw (WSAException, err);
            return Result::Failure;
        } else
        if (res != 0) {
            logE_ (_func, "closesocket(): unexpected return value: ", res);
            exc_throw (InternalException, InternalException::BackendMalfunction);
            return Result::Failure;
        }
    }

    return Result::Success;
}

}

