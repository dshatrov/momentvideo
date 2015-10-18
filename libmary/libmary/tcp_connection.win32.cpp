/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cstdio>

#include <libmary/common_win32.h>
#include <libmary/log.h>

#include <libmary/tcp_connection.h>


namespace M {

static LogGroup libMary_logGroup_tcp_conn ("tcp_conn", LogLevel::I);

#ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
AtomicInt TcpConnection::num_instances;
#endif

PollGroup::Pollable const TcpConnection::pollable = {
    processEvents,
    setFeedback,
    getFd
};

void
TcpConnection::processEvents (Uint32   const event_flags,
                              void   * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);

    logD (tcp_conn, _func, "0x", fmt_hex, (UintPtr) self);

    if (event_flags & PollGroup::Hup) {
        logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Hup");
        self->hup_received = true;
    }

    if (event_flags & PollGroup::Output) {
        if (!self->connected) {
            self->connected = true;

            int opt_val = 0;
            int opt_len = sizeof (opt_val);
            int const res = getsockopt (self->fd, SOL_SOCKET, SO_ERROR, (char*) &opt_val, &opt_len);
            if (res == SOCKET_ERROR) {
                logD_ (_func, "SOCKET_ERROR");

                WSAException wsa_exc (WSAGetLastError ());

                IoException io_exc;
                io_exc.cause = &wsa_exc;

                if (self->frontend) {
                    logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Firing \"connected\" event");
                    self->frontend.call (self->frontend->connected,
                                         /*(*/ &io_exc /*)*/);
                }

                if (self->input_frontend) {
                    self->input_frontend.call (self->input_frontend->processError,
                                               /*(*/ &io_exc /*)*/);
                }

                return;
            } else
            if (res != 0) {
                logD_ (_func, "Backend malfunction");

                InternalException internal_exc (InternalException::BackendMalfunction);

                if (self->frontend) {
                    logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Firing \"connected\" event");
                    self->frontend.call (self->frontend->connected,
                                         /*(*/ &internal_exc /*)*/);
                }

                if (self->input_frontend) {
                    self->input_frontend.call (self->input_frontend->processError,
                                               /*(*/ &internal_exc /*)*/);
                }

                return;
            }

            if (opt_val == 0) {
                logD_ (_func, "connected successfully");

                if (self->frontend) {
                    logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Firing \"connected\" event");
                    self->frontend.call (self->frontend->connected,
                                         /*(*/ (Exception*) NULL /* exc */ /*)*/);
                }
            } else {
                if (opt_val != WSAEWOULDBLOCK &&
                    opt_val != WSAEINPROGRESS)
                {
                    logD_ (_func, "opt_val: ", (unsigned long) opt_val);

                    WSAException wsa_exc (opt_val);

                    IoException io_exc;
                    io_exc.cause = &wsa_exc;

                    if (self->frontend) {
                        logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Firing \"connected\" event");
                        self->frontend.call (self->frontend->connected,
                                             /*(*/ &io_exc /*)*/);
                    }

                    if (self->input_frontend) {
                        self->input_frontend.call (self->input_frontend->processError,
                                                   /*(*/ &io_exc /*)*/);
                    }

                    return;
                } else {
		    logW_ (_func, "0x", fmt_hex, (UintPtr) self,
			   " Got output event, but not connected yet. opt_val: ", opt_val);
                }
            }
        }

        if (self->output_frontend)
            self->output_frontend.call (self->output_frontend->processOutput);
    }

    if (event_flags & PollGroup::Input ||
        event_flags & PollGroup::Hup)
    {
        if (self->input_frontend)
            self->input_frontend.call (self->input_frontend->processInput);
    }

    if (event_flags & PollGroup::Error) {
        logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Error");
        if (self->input_frontend) {
            // TODO getsockopt SO_ERROR + fill WSAException.
            IoException io_exc;
            self->input_frontend.call (self->input_frontend->processError, /*(*/ &io_exc /*)*/);
        }
    }

    if (!(event_flags & PollGroup::Input)  &&
        !(event_flags & PollGroup::Output) &&
        !(event_flags & PollGroup::Error)  &&
        !(event_flags & PollGroup::Hup))
    {
        logD_ (_func, "0x", fmt_hex, (UintPtr) self, " No events");
        return;
    }
}

void
TcpConnection::setFeedback (Cb<PollGroup::Feedback> const &feedback,
                            void * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    self->feedback = feedback;
}

SOCKET
TcpConnection::getFd (void * const _self)
{
    TcpConnection * const self = static_cast <TcpConnection*> (_self);
    return self->fd;
}

mt_throws AsyncIoResult
TcpConnection::read (Memory   const mem,
                     Size   * const ret_nread)
{
    if (ret_nread)
        *ret_nread = 0;

    if (fd == INVALID_SOCKET) {
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return AsyncIoResult::Error;
    }

    if (mem.mem() == NULL || mem.len() == 0) {
        logW_ (_func, "empty region");
        return AsyncIoResult::Normal;
    }

    WSABUF wsa_buf;
    wsa_buf.len = mem.len();
    wsa_buf.buf = (char*) mem.mem();

    Size len = mem.len();
    if ((Size) wsa_buf.len != mem.len()) {
        len = 0x10000000;
        wsa_buf.len = len;
        assert ((Size) wsa_buf.len == len);
    }

    DWORD nreceived = 0;
    DWORD flags = 0;
    int const res = WSARecv (fd, &wsa_buf, 1, &nreceived, &flags, NULL, NULL);
    if (res == SOCKET_ERROR) {
        int const error = WSAGetLastError ();
        if (error == WSAEWOULDBLOCK) {
            requestInput ();
            return AsyncIoResult::Again;
        }

        /*
        if (error == WSAEINTR)
            return AsyncIoResult::Normal;
        */

        exc_throw (WSAException, error);
        exc_push_ (IoException);
        return AsyncIoResult::Error;
    } else
    if (res != 0) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return AsyncIoResult::Error;
    }

    if (nreceived == 0)
	return AsyncIoResult::Eof;

    if (ret_nread)
        *ret_nread = (Size) nreceived;

    if ((Size) nreceived < len) {
        if (hup_received) {
            return AsyncIoResult::Normal_Eof;
        } else {
// TEST (uncomment)
//            requestInput ();
//            return AsyncIoResult::Normal_Again;
        }
    }

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
TcpConnection::write (ConstMemory   const mem,
                      Size        * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    if (fd == INVALID_SOCKET) {
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return AsyncIoResult::Error;
    }

    if (mem.mem() == NULL || mem.len() == 0) {
        logW_ (_func, "empty region");
        return AsyncIoResult::Normal;
    }

    WSABUF wsa_buf;
    wsa_buf.len = mem.len();
    wsa_buf.buf = (char*) mem.mem();

    Size len = mem.len();
    if ((Size) wsa_buf.len != len) {
        len = 0x10000000;
        wsa_buf.len = len;
        assert ((Size) wsa_buf.len == len);
    }

    DWORD nsent = 0;
    int const res = WSASend (fd, &wsa_buf, 1, &nsent, 0, NULL, NULL);
    if (res == SOCKET_ERROR) {
        int const error = WSAGetLastError ();
        if (error == WSAEWOULDBLOCK) {
            logD (tcp_conn, _func, "WSAEWOULDBLOCK, nsent: ", nsent);
            requestOutput ();
            return AsyncIoResult::Again;
        }

        /*
        if (error == WSAEINTR)
            return AsyncIoResult::Normal;
        */

        if (error == WSAECONNRESET) {
            logD_ (_func, "Eof (WSAECONNRESET)");
            return AsyncIoResult::Eof;
        }

        logD_ (_func, "Error");
        exc_throw (WSAException, error);
        exc_push_ (IoException);
        return AsyncIoResult::Error;
    } else
    if (res != 0) {
        logD_ (_func, "res != 0");
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return AsyncIoResult::Error;
    }

    if (ret_nwritten)
        *ret_nwritten = (Size) nsent;

    return AsyncIoResult::Normal;
}

mt_throws Result
TcpConnection::doClose ()
{
    if (fd != INVALID_SOCKET) {
        // A call to shutdown() is needed on Windows, or there'll be data loss.
        // That's stated in multiple sources, including msdn.
        //
        // Note that we may call shutdown() for unconnected sockets here.
        // There's no reason to care about this, so we just ignore any errors.
        if (shutdown (fd, SD_BOTH))
            logD_ (_this_func, "shutdown() failed: ", wsaErrorToString (WSAGetLastError()));

        int const res = ::closesocket (fd);
        if (res == SOCKET_ERROR) {
            exc_throw (WSAException, WSAGetLastError());
            return Result::Failure;
        } else
        if (res != 0) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            return Result::Failure;
        }
    }

    return Result::Success;
}

mt_throws Result
TcpConnection::open ()
{
    if (!_libMary_win32_socket_tcp (&fd, false /* overlapped */))
        return Result::Failure;

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
            logE_ (_func, "ioctlsocket() failed: " /* TODO , wsaErrorToString (error) */);
            return Result::Failure;
        }
    }

    if (!_libMary_win32_setsockopt_nodelay (fd))
        return Result::Failure;

    return Result::Success;
}

mt_throws TcpConnection::ConnectResult
TcpConnection::connect (IpAddress const addr)
{
    struct sockaddr_in saddr;
    setIpAddress (addr, &saddr);
    for (;;) {
	int const res = ::connect (fd, (struct sockaddr*) &saddr, sizeof (saddr));
	if (res == 0) {
            logD_ (_func, "connected successfully");

	    connected = true;
            return ConnectResult_Connected;
	} else
	if (res == SOCKET_ERROR) {
            int const error = WSAGetLastError ();
            if (error == WSAEWOULDBLOCK)
                break;

	    exc_throw (WSAException, error);
	    exc_push_ (IoException);
	    logE_ (_func, "connect() failed: " /* TODO , wsaErrorToString (error) */);
            return ConnectResult_Error;
	} else {
	    exc_throw (InternalException, InternalException::BackendMalfunction);
	    logE_ (_func, "connect(): unexpected return value ", res);
            return ConnectResult_Error;
	}

	break;
    }

    return ConnectResult_InProgress;
}

TcpConnection::TcpConnection (EmbedContainer * const embed_container)
    : Object       (embed_container),
      fd           (INVALID_SOCKET),
      connected    (false),
      hup_received (false)
{
#ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
    fprintf (stderr, " TcpConnection(): 0x%lx, num_instances: %d\n",
             (unsigned long) this,
	     (int) (num_instances.fetchAdd (1) + 1));
#endif
}

TcpConnection::~TcpConnection ()
{
    logD_ (_func, "0x", fmt_hex, (UintPtr) this);

#ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
    fprintf (stderr, "~TcpConnection(): 0x%lx, num_instances: %d\n",
	     (unsigned long) this,
	     (int) (num_instances.fetchAdd (-1) - 1));
#endif

    if (fd != INVALID_SOCKET) {
        if (!doClose ())
            logE_ (_func, "close() failed: ", exc->toString());
    }
}

}

