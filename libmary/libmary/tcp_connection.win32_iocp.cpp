/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <mswsock.h>

#include <libmary/common_win32.h>

#include <libmary/tcp_connection.h>


namespace M {

PollGroup::Pollable const TcpConnection::pollable = {
    getFd
};

SOCKET
TcpConnection::getFd (void * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    return self->socket_fd;
}

void
TcpConnection::connectIoComplete (Exception  * const exc_,
                                  Overlapped * const mt_nonnull overlapped,
                                  Size         const /* bytes_transferred */,
                                  void       * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    assert (overlapped == self->connect_overlapped);

    logD_ (_self_func, "exc_: 0x", fmt_hex, (UintPtr) exc_);

    if (!self->connected) {
        self->connected = true;

        if (!exc_) {
            // Needed for shutdown() to work.
            if (setsockopt (self->socket_fd,
                            SOL_SOCKET,
                            SO_UPDATE_CONNECT_CONTEXT,
                            NULL /* optval */,
                            0    /* optlen */))
            {
                logW_ (_func, "setsockopt(SOL_SOCKET, SO_UPDATE_CONNET_CONTEXT) "
                       "failed: ", wsaErrorToString (WSAGetLastError()));
            }
        }

        if (self->frontend)
            self->frontend.call (self->frontend->connected, exc_);
    } else {
        logW_ (_self_func, "Spurious connectIoComplete event");
    }
}

mt_throws AsyncIoResult
TcpConnection::read (OVERLAPPED * const mt_nonnull overlapped,
                     Memory       const mem,
                     Size       * const ret_nread)
{
    memset (overlapped, 0, sizeof (OVERLAPPED));

    WSABUF buffers [1];
    buffers [0].buf = (char*) mem.mem();
    buffers [0].len = mem.len();

    DWORD bytes_received = 0;
    DWORD flags = 0;
    int const res = WSARecv (socket_fd,
                             buffers,
                             1    /* dwBufferCount */,
                             &bytes_received,
                             &flags,
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

    if (ret_nread)
        *ret_nread = (Size) bytes_received;

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
TcpConnection::write (OVERLAPPED  * const mt_nonnull overlapped,
                      ConstMemory   const mem,
                      Size        * const ret_nwritten)
{
    WSABUF buffers [1];
    buffers [0].buf = (char*) mem.mem();
    buffers [0].len = mem.len();
    return writev (overlapped, buffers, 1, ret_nwritten);
}

mt_throws AsyncIoResult
TcpConnection::writev (OVERLAPPED * const mt_nonnull overlapped,
                       WSABUF     * const mt_nonnull buffers,
                       Count        const buffer_count,
                       Size       * const ret_nwritten)
{
    memset (overlapped, 0, sizeof (OVERLAPPED));

    DWORD bytes_sent = 0;
    int const res = WSASend (socket_fd,
                             buffers,
                             (DWORD) buffer_count,
                             &bytes_sent,
                             0    /* dwFlags */,
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

mt_throws Result
TcpConnection::getLocalAddress (IpAddress * const mt_nonnull ret_addr)
{
    struct sockaddr_in saddr;
    memset (&saddr, 0, sizeof (saddr));
    int saddr_len = sizeof (saddr);

    int const res = getsockname (socket_fd, (struct sockaddr*) &saddr, &saddr_len);
    if (res == SOCKET_ERROR) {
        int const err = WSAGetLastError();
        logE_ (_this_func, "getsockname() failed: ", wsaErrorToString (err));
        exc_throw (WSAException, err);
        return Result::Failure;
    } else
    if (res != 0) {
        logE_ (_this_func, "getsockname(): unexpected return value: ", res);
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return Result::Failure;
    }

    setIpAddress (&saddr, ret_addr);
    return Result::Success;
}

mt_const mt_throws Result
TcpConnection::open ()
{
    if (!_libMary_win32_socket_tcp (&socket_fd, true /* overlapped */))
        return Result::Failure;

    if (!_libMary_win32_setsockopt_nodelay (socket_fd))
        return Result::Failure;

    return Result::Success;
}

mt_const mt_throws TcpConnection::ConnectResult
TcpConnection::connect (IpAddress const addr)
{
    {
        IpAddress local_addr;
        if (!setIpAddress (ConstMemory(), 0, &local_addr)) {
            logE_ (_func, "setIpAddress() failed");
            exc_throw (InternalException, InternalException::BackendMalfunction);
            return ConnectResult_Error;
        }

        if (!_libMary_win32_bind (socket_fd, local_addr)) {
            logE_ (_func, "bind() failed: ", exc->toString());
            return ConnectResult_Error;
        }
    }

    LPFN_CONNECTEX ptr_ConnectEx = NULL;
    {
        GUID guid = WSAID_CONNECTEX;
        DWORD num_bytes = 0;
        int const res = WSAIoctl (socket_fd,
                                  SIO_GET_EXTENSION_FUNCTION_POINTER,
                                  &guid,
                                  sizeof (guid),
                                  &ptr_ConnectEx,
                                  sizeof (ptr_ConnectEx),
                                  &num_bytes,
                                  NULL /* lpOverlapped */,
                                  NULL /* lpCompletionRoutine */);
        if (res != 0) {
            int const err = WSAGetLastError();
            exc_throw (WSAException, err);
            logE_ (_func, "WSAIoctl() failed");
            return ConnectResult_Error;
        }

        if (!ptr_ConnectEx) {
            exc_throw (InternalException, InternalException::BackendError);
            logE_ (_func, "Could not get ConnectEx function pointer");
            return ConnectResult_Error;
        }
    }

    struct sockaddr_in saddr;
    setIpAddress (addr, &saddr);

    connect_overlapped->ref ();

    OVERLAPPED * const sys_overlapped = connect_overlapped;
    memset (sys_overlapped, 0, sizeof (OVERLAPPED));

    BOOL const res = ptr_ConnectEx (socket_fd,
                                    (struct sockaddr*) &saddr,
                                    sizeof (saddr),
                                    NULL /* lpSendBuffer */,
                                    0    /* dwSendDataLength */,
                                    NULL /* lpdwBytesSent */,
                                    sys_overlapped);
    if (!res) {
        int const err = WSAGetLastError();
        if (err == ERROR_IO_PENDING || err == WSA_IO_PENDING)
            return ConnectResult_InProgress;

        connect_overlapped->unref ();

        logE_ (_func, "ConnectEx() failed: ", wsaErrorToString (err));
        exc_throw (WSAException, err);
        return ConnectResult_Error;
    }

    return ConnectResult_Connected;
}

TcpConnection::TcpConnection (EmbedContainer * const embed_container)
    : Object    (embed_container),
      socket_fd (INVALID_SOCKET),
      connected (false)
{
    connect_overlapped = grab (new (std::nothrow) Overlapped);
    connect_overlapped->io_complete_cb =
            CbDesc<Overlapped::IoCompleteCallback> (&connectIoComplete, this, this);
}

TcpConnection::~TcpConnection ()
{
    if (socket_fd != INVALID_SOCKET) {
        // A call to shutdown() is needed on Windows, or there'll be data loss.
        // That's stated in multiple sources, including msdn.
        //
        // Note that we may call shutdown() for unconnected sockets here.
        // There's no reason to care about this, so we just ignore any errors.
        if (shutdown (socket_fd, SD_BOTH))
            logD_ (_this_func, "shutdown() failed: ", wsaErrorToString (WSAGetLastError()));
    }

    _libMary_win32_closesocket (socket_fd);
}

}

