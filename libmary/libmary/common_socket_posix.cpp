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

#include <libmary/util_str.h>
#include <libmary/util_posix.h>
#include <libmary/log.h>

#include <libmary/common_socket_posix.h>


namespace M {

AsyncIoResult
CommonSocket::read (Memory   const mem,
                    Size   * const ret_nread)
    mt_throw ((IoException,
               InternalException))
{
    if (ret_nread)
        *ret_nread = 0;

    Size len;
    if (mem.len() > SSIZE_MAX)
        len = SSIZE_MAX;
    else
        len = mem.len();

    ssize_t const res = recv (fd, mem.mem(), (ssize_t) len, 0 /* flags */);
    if (res == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            requestInput ();
            return AsyncIoResult::Again;
        }

        if (errno == EINTR)
            return AsyncIoResult::Normal;

        exc_throw (PosixException, errno);
        exc_push_ (IoException);
        return AsyncIoResult::Error;
    } else
    if (res < 0) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return AsyncIoResult::Error;
    } else
    if (res == 0) {
        return AsyncIoResult::Eof;
    }

    if (ret_nread)
        *ret_nread = (Size) res;

    if ((Size) res < len) {
        if (hup_received) {
            return AsyncIoResult::Normal_Eof;
        } else {
            requestInput ();

            if (auto_normal_again)
                return AsyncIoResult::Normal_Again;
        }
    }

    return AsyncIoResult::Normal;
}

AsyncIoResult
CommonSocket::write (ConstMemory   const mem,
                     Size        * const ret_nwritten)
    mt_throw ((IoException,
               InternalException))
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    Size len;
    if (mem.len() > SSIZE_MAX)
        len = SSIZE_MAX;
    else
        len = mem.len();

    ssize_t const res = send (fd, mem.mem(), (ssize_t) len,
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
            return AsyncIoResult::Normal;

        if (errno == EPIPE)
            return AsyncIoResult::Eof;

        exc_throw (PosixException, errno);
        exc_push_ (IoException);
        return AsyncIoResult::Error;
    } else
    if (res < 0) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return AsyncIoResult::Error;
    }

    if (ret_nwritten)
        *ret_nwritten = (Size) res;

    return AsyncIoResult::Normal;
}

mt_throws AsyncIoResult
CommonSocket::writev (struct iovec * const iovs,
                      Count          const num_iovs,
                      Size         * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

#ifndef LIBMARY_TEST_MWRITEV
    ssize_t const res = ::writev (fd, iovs, num_iovs);
#else
    ssize_t res;
    if (libMary_mwritevAvailable()) {
#ifndef LIBMARY_TEST_MWRITEV_SINGLE
        int w_fd = fd;
        struct iovec *w_iovs = iovs;
        int w_num_iovs = num_iovs;
        int w_res;
        if (!libMary_mwritev (1, &w_fd, &w_iovs, &w_num_iovs, &w_res)) {
#else
        int w_res;
        if (!libMary_mwritevSingle (fd, iovs, num_iovs, &w_res)) {
#endif // LIBMARY_TEST_MWRITEV_SINGLE
            res = -1;
            errno = EINVAL;
        } else {
            if (w_res >= 0) {
                res = w_res;
            } else {
                res = -1;
                errno = -w_res;
            }
        }
    } else {
        res = ::writev (fd, iovs, num_iovs);
    }
#endif // LIBMARY_TEST_MWRITEV
    if (res == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            requestOutput ();
            return AsyncIoResult::Again;
        }

        if (errno == EINTR)
            return AsyncIoResult::Normal;

        if (errno == EPIPE)
            return AsyncIoResult::Eof;

        exc_throw (PosixException, errno);
        exc_push (InternalException, InternalException::BackendError);
        logE_ (_func, "writev() failed (num_iovs ", num_iovs, "): ", errnoString (errno));
#if 0
        // Dump of all iovs.
        for (Count i = 0; i < num_iovs; ++i) {
            logE_ (_func, "iovs[", i, "]: ", fmt_hex, (UintPtr) iovs [i].iov_base, ", ", fmt_def, iovs [i].iov_len);
        }
#endif

        return AsyncIoResult::Error;
    } else
    if (res < 0) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        logE_ (_func, "writev(): unexpected return value: ", res);
        return AsyncIoResult::Error;
    }

    if (ret_nwritten)
        *ret_nwritten = (Size) res;

    return AsyncIoResult::Normal;
}

CommonSocket::CommonSocket (EmbedContainer * const embed_container,
                            bool             const auto_normal_again)
    : Connection        (embed_container),
      auto_normal_again (auto_normal_again),
      fd                (-1),
      hup_received      (false)
{
}

CommonSocket::~CommonSocket ()
{
    posixClose (fd, _func);
}

}

