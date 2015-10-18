/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/common_win32.h>
#include <libmary/log.h>

#include <libmary/tcp_server.h>


namespace M {

mt_sync_domain (acceptIoComplete)
mt_throws Result
TcpServer::initiateAccept ()
{
    logD_ (_this_func_);

    assert (ptr_AcceptEx);
    assert (conn_socket == INVALID_SOCKET);

    for (;;) {
        if (!_libMary_win32_socket_tcp (&conn_socket, true /* overlapped */))
            return Result::Failure;

        server_overlapped->ref ();

        OVERLAPPED * const sys_overlapped = server_overlapped;
        memset (sys_overlapped, 0, sizeof (OVERLAPPED));

        DWORD bytes_received = 0;
        BOOL const res = ptr_AcceptEx (socket_fd,
                                       conn_socket,
                                       server_overlapped->out_buf,
                                       0 /* dwReceiveDataLength */,
                                       sizeof (sockaddr_in) + 16,
                                       sizeof (sockaddr_in) + 16,
                                       &bytes_received,
                                       sys_overlapped);
        if (!res) {
            int const err = WSAGetLastError();
            if (err == ERROR_IO_PENDING || err == WSA_IO_PENDING)
                return Result::Success;

            _libMary_win32_closesocket (conn_socket);
            server_overlapped->unref ();

            logE_ (_this_func, "AcceptEx() failed: ", wsaErrorToString (err));

          // Looping because WSAECONNRESET is a non-failure, and other
          // AcceptEx error codes are not precisely documented.
          //
          // We want to return from initiateAccept() knowing that there'll be
          // a subsequent completion notification.
        } else {
            break;
        }
    }

    return Result::Success;
}

PollGroup::Pollable const TcpServer::pollable = {
    getFd
};

SOCKET
TcpServer::getFd (void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    return self->socket_fd;
}

bool
TcpServer::startTask (void * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);

    logD_ (_self_func_);

    if (!self->initiateAccept ())
        logF_ (_self_func, "initiateAccept() failed: ", exc->toString());

    return false /* do not reschedule */;
}

void
TcpServer::acceptIoComplete (Exception  * const exc_,
                             Overlapped * const mt_nonnull overlapped,
                             Size         const /* bytes_transferred */,
                             void       * const _self)
{
    TcpServer * const self = static_cast <TcpServer*> (_self);
    assert (overlapped == self->server_overlapped);

    logD_ (_self_func_);

    assert (!self->accepted);
    assert (self->conn_socket != INVALID_SOCKET);

    if (!exc_) {
        // Needed for shutdown() to work.
        if (setsockopt (self->conn_socket,
                        SOL_SOCKET,
                        SO_UPDATE_ACCEPT_CONTEXT,
                        (char*) &self->socket_fd,
                        sizeof (self->socket_fd)))
        {
            logW_ (_self_func, "setsockopt(SOL_SOCKET, SO_UPDATE_CONNET_CONTEXT) "
                   "failed: ", wsaErrorToString (WSAGetLastError()));
        }

        self->accepted = true;
        if (self->frontend)
            self->frontend.call (self->frontend->accepted);
    } else {
        _libMary_win32_closesocket (self->conn_socket);
        self->conn_socket = INVALID_SOCKET;
        if (!self->initiateAccept ())
            logF_ (_self_func, "initiateAccept() failed: ", exc->toString());
    }
}

mt_throws Result
TcpServer::open ()
{
    logD_ (_this_func_);

    socket_fd = WSASocket (AF_INET,
                           SOCK_STREAM,
                           IPPROTO_TCP,
                           NULL /* lpProtocolInfo */,
                           0    /* group */,
                           WSA_FLAG_OVERLAPPED);
    if (socket_fd == INVALID_SOCKET) {
        int const err = WSAGetLastError();
        logF_ (_this_func, "WSASocket() failed: ", wsaErrorToString (err));
        exc_throw (WSAException, err);
        return Result::Failure;
    }

    if (!_libMary_win32_setsockopt_reuseaddr (socket_fd))
        return Result::Failure;

    {
        GUID guid = WSAID_ACCEPTEX;
        DWORD num_bytes = 0;
        int const res = WSAIoctl (socket_fd,
                                  SIO_GET_EXTENSION_FUNCTION_POINTER,
                                  &guid,
                                  sizeof (guid),
                                  &ptr_AcceptEx,
                                  sizeof (ptr_AcceptEx),
                                  &num_bytes,
                                  NULL /* lpOverlapped */,
                                  NULL /* lpCompletionRoutine */);
        if (res != 0) {
            int const err = WSAGetLastError();
            logF_ (_this_func, "WSAIoctl() failed: ", wsaErrorToString (err));

            ptr_AcceptEx = NULL;

            exc_throw (WSAException, err);
            return Result::Failure;
        }

        if (!ptr_AcceptEx) {
            logF_ (_this_func, "Could not get AcceptEx function pointer");
            exc_throw (InternalException, InternalException::BackendError);
            return Result::Failure;
        }
    }

    {
        GUID guid = WSAID_GETACCEPTEXSOCKADDRS;
        DWORD num_bytes = 0;
        int const res = WSAIoctl (socket_fd,
                                  SIO_GET_EXTENSION_FUNCTION_POINTER,
                                  &guid,
                                  sizeof (guid),
                                  &ptr_GetAcceptExSockaddrs,
                                  sizeof (ptr_GetAcceptExSockaddrs),
                                  &num_bytes,
                                  NULL /* lpOverlapped */,
                                  NULL /* lpCompletionRoutine */);
        if (res != 0) {
            int const err = WSAGetLastError();
            logF_ (_this_func, "WSAIoctl() failed: ", wsaErrorToString (err));

            ptr_GetAcceptExSockaddrs = NULL;

            exc_throw (WSAException, err);
            return Result::Failure;
        }

        if (!ptr_GetAcceptExSockaddrs) {
            logF_ (_this_func, "Could not get GetAcceptExSockaddrs function pointer");
            exc_throw (InternalException, InternalException::BackendError);
            return Result::Failure;
        }
    }

    return Result::Success;
}

mt_sync_domain (acceptIoComplete)
mt_throws TcpServer::AcceptResult
TcpServer::accept (TcpConnection * const mt_nonnull tcp_conn,
                   IpAddress     * const ret_addr)
{
    logD_ (_this_func_);

    assert (ptr_GetAcceptExSockaddrs);

    if (!accepted) {
        return AcceptResult::NotAccepted;
    }
    accepted = false;

    assert (conn_socket != INVALID_SOCKET);
    tcp_conn->setSocket (conn_socket);

    if (ret_addr) {
        SOCKADDR *local_saddr  = NULL;
        SOCKADDR *remote_saddr = NULL;
        int local_saddr_len  = 0;
        int remote_saddr_len = 0;

        ptr_GetAcceptExSockaddrs (server_overlapped->out_buf,
                                  0 /* dwReceiveDataLength */,
                                  sizeof (sockaddr_in) + 16,
                                  sizeof (sockaddr_in) + 16,
                                  &local_saddr,
                                  &local_saddr_len,
                                  &remote_saddr,
                                  &remote_saddr_len);

        setIpAddress ((struct sockaddr_in*) remote_saddr, ret_addr);

        logD_ (_this_func, "client address: ", *ret_addr);
    }

    conn_socket = INVALID_SOCKET;
    if (!initiateAccept ())
        logF_ (_this_func, "initiateAccept() failed: ", exc->toString());

    return AcceptResult::Accepted;
}

mt_throws Result
TcpServer::bind (IpAddress const addr)
    { return _libMary_win32_bind (socket_fd, addr); }

mt_throws Result
TcpServer::listen ()
    { return _libMary_win32_listen (socket_fd); }

mt_throws Result
TcpServer::start ()
{
    logD_ (_this_func_);
    deferred_reg.scheduleTask (&start_task, false /* permanent */);
    return Result::Success;
}

void
TcpServer::init (CbDesc<Frontend> const &frontend,
                 DeferredProcessor * const mt_nonnull deferred_processor,
                 Timers            * const mt_nonnull /* timers */,
                 Time                const /* accept_retry_timeout_millisec */)
{
    this->frontend = frontend;
    deferred_reg.setDeferredProcessor (deferred_processor);
}

TcpServer::TcpServer (EmbedContainer * const embed_container)
    : Object       (embed_container),
      socket_fd    (INVALID_SOCKET),
      ptr_AcceptEx (NULL),
      ptr_GetAcceptExSockaddrs (NULL),
      conn_socket  (INVALID_SOCKET),
      accepted     (false)
{
    start_task.cb = CbDesc<DeferredProcessor::TaskCallback> (startTask, this, this);

    server_overlapped = grab (new (std::nothrow) ServerOverlapped);
    server_overlapped->io_complete_cb =
            CbDesc<Overlapped::IoCompleteCallback> (&acceptIoComplete, this, this);
}

TcpServer::~TcpServer ()
{
    _libMary_win32_closesocket (socket_fd);
  // We don't close 'conn_socket' to avoid extra synchronization.

    deferred_reg.release ();
}

}

