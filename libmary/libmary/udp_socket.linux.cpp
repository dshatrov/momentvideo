/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
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
#include <netinet/in.h>

#include <libmary/util_posix.h>

#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/udp_socket.linux.h>
#else
  #include <libmary/udp_socket.h>
#endif


namespace M {    
    
PollGroup::Pollable const UdpSocket::pollable = {
    processEvents,
    setFeedback,
    getFd
};

void
UdpSocket::processEvents (Uint32   const event_flags,
                          void   * const _self)
{
    UdpSocket * const self = static_cast <UdpSocket*> (_self);

    if (event_flags & PollGroup::Hup) {
        logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Hup");
        self->hup_received = true;
    }

    if (event_flags & PollGroup::Output) {
        if (self->output_frontend && self->output_frontend->processOutput)
            self->output_frontend.call (self->output_frontend->processOutput);
    }

    if (event_flags & PollGroup::Input ||
        event_flags & PollGroup::Hup)
    {
        if (self->input_frontend && self->input_frontend->processInput)
            self->input_frontend.call (self->input_frontend->processInput);
    }

    if (event_flags & PollGroup::Error) {
        logD_ (_func, "0x", fmt_hex, (UintPtr) self, " Error");
        if (self->input_frontend && self->input_frontend->processError) {
            // TODO getsockopt SO_ERROR + fill PosixException
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

int
UdpSocket::getFd (void * const _self)
{
    UdpSocket * const self = static_cast <UdpSocket*> (_self);
    return self->fd;
}

void
UdpSocket::setFeedback (Cb<PollGroup::Feedback> const &feedback,
                        void * const _self)
{
    UdpSocket * const self = static_cast <UdpSocket*> (_self);
    self->feedback = feedback;
}

AsyncIoResult
UdpSocket::sendVectorTo (IpAddress      const addr,
                         struct iovec * const mt_nonnull iovs,
                         Count          const num_iovs,
                         Size         * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    struct msghdr hdr;
    memset (&hdr, 0, sizeof (hdr));

    struct sockaddr_in sys_addr;
    setIpAddress (addr, &sys_addr);

    hdr.msg_name = &sys_addr;
    hdr.msg_namelen = sizeof (sys_addr);
    hdr.msg_iov = iovs;
    hdr.msg_iovlen = num_iovs;
    hdr.msg_control = NULL;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;

    for (;;) {
        ssize_t const res = sendmsg (fd, &hdr,
              #ifdef __MACH__
                0
              #else
                MSG_NOSIGNAL
              #endif
                );
        if (res == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                requestOutput ();
                return AsyncIoResult::Again;
            }

            if (errno == EINTR)
                continue;

            if (errno == EPIPE)
                return AsyncIoResult::Eof;

            exc_throw (PosixException, errno);
            logE_ (_func, "sendmsg() failed: ", errnoString (errno));

            return AsyncIoResult::Error;
        } else
        if (res < 0) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            logE_ (_func, "sendmsg(): unexpected return value: ", res);
            return AsyncIoResult::Error;
        }

        if (ret_nwritten)
            *ret_nwritten = (Size) res;

        break;
    }

    return AsyncIoResult::Normal;
}

mt_const mt_throws Result
UdpSocket::open ()
{
    posixClose (fd, _func);

    fd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1) {
        exc_throw (PosixException, errno);
        exc_push (InternalException, InternalException::BackendError);
        logE_ (_func, "socket() failed: ", errnoString (errno));
        return Result::Failure;
    }

    {
        int flags = fcntl (fd, F_GETFL, 0);
        if (flags == -1) {
            exc_throw (PosixException, errno);
            exc_push (InternalException, InternalException::BackendError);
            logE_ (_func, "fcntl() failed (F_GETFL): ", errnoString (errno));
            return Result::Failure;
        }

        flags |= O_NONBLOCK;

        if (fcntl (fd, F_SETFL, flags) == -1) {
            exc_throw (PosixException, errno);
            exc_push (InternalException, InternalException::BackendError);
            logE_ (_func, "fcntl() failed (F_SETFL): ", errnoString (errno));
            return Result::Failure;
        }
    }

    return Result::Success;
}

mt_const mt_throws Result
UdpSocket::connect (IpAddress const addr)
{
    struct sockaddr_in saddr;
    setIpAddress (addr, &saddr);
    for (;;) {
        int const res = ::connect (fd, (struct sockaddr*) &saddr, sizeof (saddr));
        if (res == -1) {
            if (errno == EINTR)
                continue;

            exc_throw (PosixException, errno);
            exc_push (InternalException, InternalException::BackendError);
            logE_ (_func, "connect() failed: ", errnoString (errno));
            return Result::Failure;
        } else {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            logE_ (_func, "connect(): unexpected return value ", res);
            return Result::Failure;
        }

        break;
    }

    return Result::Success;
}

mt_const mt_throws Result
UdpSocket::bind (IpAddress const addr)
{
    struct sockaddr_in sys_addr;
    setIpAddress (addr, &sys_addr);

    int const res = ::bind (fd, (struct sockaddr*) &sys_addr, sizeof (sys_addr));
    if (res == -1) {
        exc_throw (PosixException, errno);
        exc_push (InternalException, InternalException::BackendError);
        logE_ (_this_func, "bind(", addr, ") failed: ", errnoString (errno));
        return Result::Failure;
    } else
    if (res != 0) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        logE_ (_this_func, "bind(", addr, "): unexpected return value: ", res);
        return Result::Failure;
    }

    return Result::Success;
}

mt_const mt_throws Result
UdpSocket::bindToAnyLocalPort ()
{
    IpAddress addr;
    if (!setIpAddress (ConstMemory(), 0, &addr)) {
        logE_ (_this_func, "setIpAddress (\"\", 0) failed");
        exc_throw (InternalException, InternalException::BackendError);
        return Result::Failure;
    }

    return bind (addr);
}

UdpSocket::UdpSocket (EmbedContainer * const embed_container)
    : CommonSocket (embed_container, false /* auto_normal_again */)
{
}

UdpSocket::~UdpSocket ()
{
}

}

