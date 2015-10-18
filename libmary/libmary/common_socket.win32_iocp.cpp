/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/common_win32.h>

#include <libmary/common_socket.win32_iocp.h>


namespace M {

mt_throws AsyncIoResult
CommonSocket::read (OVERLAPPED * const mt_nonnull overlapped,
                    Memory       const mem,
                    Size       * const ret_nread)
{
    if (ret_nread)
        *ret_nread = 0;

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
CommonSocket::write (OVERLAPPED  * const mt_nonnull overlapped,
                     ConstMemory   const mem,
                     Size        * const ret_nwritten)
{
    WSABUF buffers [1];
    buffers [0].buf = (char*) mem.mem();
    buffers [0].len = mem.len();
    return writev (overlapped, buffers, 1, ret_nwritten);
}

mt_throws AsyncIoResult
CommonSocket::writev (OVERLAPPED * const mt_nonnull overlapped,
                      WSABUF     * const mt_nonnull buffers,
                      Count        const buffer_count,
                      Size       * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

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

CommonSocket::CommonSocket ()
    : Object    (NULL /* embed_container */),
      socket_fd (INVALID_SOCKET)
{
}

CommonSocket::~CommonSocket ()
{
    _libMary_win32_closesocket (socket_fd);
}

}

