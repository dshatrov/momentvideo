/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_POSIX__H__
#define LIBMARY__UTIL_POSIX__H__


#include <libmary/types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <libmary/exception.h>
#include <libmary/log.h>


namespace M {

#ifndef LIBMARY_PLATFORM_WIN32
  mt_throws Result posix_createNonblockingPipe (int (*fd) [2]);

  mt_throws Result commonTriggerPipeWrite (int fd);
  mt_throws Result commonTriggerPipeRead  (int fd);
#endif

mt_throws Result posix_statToFileStat (struct stat * mt_nonnull stat_buf,
                                       FileStat    * mt_nonnull ret_stat);

template <class ...Args>
void posixClose (int const fd, Args const &...args)
{
    if (fd != -1) {
        for (;;) {
            int const res = ::close (fd);
            if (res == -1) {
                /* There're reports that retrying close() on EINTR may cause
                 * spurious closures of random fds (i.e. fd may be actually
                 * closed and reused when close() returns EINTR).
                 *
                 * Latest close(2) Linux man page tells the same.
                 *
                 * But this is not the case on some other unixes (e.g. HPUX).
                 * See http://ewontfix.com/4/
                 *
                if (errno == EINTR)
                    continue;
                */

                logE_ (args..., "close() failed: ", errnoString (errno));
            } else
            if (res != 0) {
                logE_ (args..., "close(): unexpected return value: ", res);
            }

            break;
        }
    }
}

template <class ...Args>
mt_throws Result posixClose_nolog (int const fd, Args const &.../* args */)
{
    if (fd != -1) {
        // No EINTR loop here. See comments in posixClose() for details.
        int const res = ::close (fd);
        if (res == -1) {
            exc_throw (PosixException, errno);
            return Result::Failure;
        } else
        if (res != 0) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            return Result::Failure;
        }
    }

    return Result::Success;
}

}


#endif /* LIBMARY__UTIL_POSIX__H__ */

