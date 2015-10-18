/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <mswsock.h>

#include <libmary/common_win32.h>

#include <libmary/udp_socket.h>


namespace M {

PollGroup::Pollable const UdpSocket::pollable = {
    getFd
};

SOCKET
UdpSocket::getFd (void * const _self)
{
    UdpSocket * const self = static_cast <UdpSocket*> (_self);
    return self->socket_fd;
}

AsyncIoResult
UdpSocket::sendVectorTo (IpAddress    const addr,
                         OVERLAPPED * const mt_nonnull overlapped,
                         WSABUF     * const mt_nonnull buffers,
                         Count        const buf_count,
                         Size       * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    memset (overlapped, 0, sizeof (OVERLAPPED));

    struct sockaddr_in sys_addr;
    setIpAddress (addr, &sys_addr);

    DWORD bytes_sent = 0;
    int const res = WSASendTo (socket_fd,
                               buffers,
                               (DWORD) buf_count,
                               &bytes_sent,
                               0 /* dwFlags */,
                               (struct sockaddr*) &sys_addr,
                               sizeof (sys_addr),
                               overlapped,
                               NULL /* lpCompletionRoutine */);
    if (res == SOCKET_ERROR) {
        int const err = WSAGetLastError();
        if (err == WSA_IO_PENDING)
            return AsyncIoResult::Again;
        // WSAEWOULDBLOCK may be relevant as well.

        exc_throw (WSAException, err);
        return AsyncIoResult::Error;
    } else
    if (res != 0) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return AsyncIoResult::Error;
    }

    if (ret_nwritten)
        *ret_nwritten = (Size) bytes_sent;

    return AsyncIoResult::Normal;
}

mt_const mt_throws Result
UdpSocket::open ()
{
    _libMary_win32_closesocket (socket_fd);

    socket_fd = WSASocket (AF_INET,
                           SOCK_DGRAM,
                           IPPROTO_UDP,
                           NULL /* lpProtocolInfo */,
                           0    /* group */,
                           WSA_FLAG_OVERLAPPED);
    if (socket_fd == INVALID_SOCKET) {
        int const err = WSAGetLastError();
	logF_ (_func, "WSASocket() failed: ", wsaErrorToString (err));
	exc_throw (WSAException, err);
        return Result::Failure;
    }

    return Result::Success;
}

#if 0
// Unused
mt_const mt_throws Result
UdpSocket::connect (IpAddress const addr)
{
}
#endif

mt_const mt_throws Result
UdpSocket::bind (IpAddress const addr)
{
    return _libMary_win32_bind (socket_fd, addr);
}

#if 0
// Unused
mt_const mt_throws Result
UdpSocket::bindToAnyLocalPort ()
{
}
#endif

UdpSocket::UdpSocket (EmbedContainer * const embed_container)
    : Object (embed_container)
{
}

UdpSocket::~UdpSocket ()
{
}

}

