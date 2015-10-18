/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifdef __linux__
// For lseek64()
#define _LARGEFILE64_SOURCE
#endif

#include <libmary/types.h>

#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <libmary/util_base.h>
#include <libmary/util_time.h>
#include <libmary/util_posix.h>
#include <libmary/util_dev.h>

#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/native_file.linux.h>
#else
  #include <libmary/native_file.h>
#endif


// NOTE: Don't call log*() functions here. NativeFile is a part of logging call chain.


namespace M {

mt_throws IoResult
NativeFile::read (Memory   const mem,
		  Size   * const ret_nread)
{
    if (ret_nread)
	*ret_nread = 0;

    // According to POSIX, if we pass a value larger than SSIZE_MAX to read,
    // then the result is implementation-defined.
    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = ::read (fd, mem.mem(), len);
    if (res == -1) {
	if (errno == EINTR)
	    return IoResult::Normal;

	exc_throw (PosixException, errno);
	exc_push_ (IoException);
	return IoResult::Error;
    } else
    if (res < 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return IoResult::Error;
    } else
    if (res == 0) {
	return IoResult::Eof;
    }

    if ((Size) res > len) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return IoResult::Error;
    }

    if (ret_nread)
	*ret_nread = res;

    return IoResult::Normal;
}

mt_throws Result
NativeFile::write (ConstMemory   const mem,
		   Size        * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    // According to POSIX, if we pass a value larger than SSIZE_MAX to read,
    // then the result is implementation-defined.
    Size len;
    if (mem.len() > SSIZE_MAX)
	len = SSIZE_MAX;
    else
	len = mem.len();

    ssize_t const res = ::write (fd, mem.mem(), len);

    if (res == -1) {
	if (errno == EINTR)
	    return Result::Success;

	if (errno == EPIPE) {
	  // If there'll be a need to distinguish Eof from Error, then this
	  // is the place to intervene.
            exc_throw (PosixException, errno);
            exc_push_ (IoException);
	    return Result::Failure;
	}

	exc_throw (PosixException, errno);
	exc_push_ (IoException);
	return Result::Failure;
    } else
    if (res < 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return Result::Failure;
    }

    if ((Size) res > len) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return Result::Failure;
    }

    if (ret_nwritten)
	*ret_nwritten = res;

    return Result::Success;
}

mt_throws Result
NativeFile::seek (FileOffset const offset,
		  SeekOrigin const origin)
{
    int whence = 0;

    switch ((SeekOrigin::Value) origin) {
        case SeekOrigin::Beg:
            whence = SEEK_SET;
            break;
        case SeekOrigin::Cur:
            whence = SEEK_CUR;
            break;
        case SeekOrigin::End:
            whence = SEEK_END;
            break;
    }

  #ifdef __linux__
    /* NOTE: lseek64() seems to be glibc-specific. */
    if (lseek64 (fd, (off64_t) offset, whence) == (off64_t) -1) {
  #else
    if (lseek (fd, (off_t) offset, whence) == (off_t) -1) {
  #endif
        exc_throw (PosixException, errno);
        return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
NativeFile::tell (FileSize * const ret_pos)
{
    if (ret_pos)
        *ret_pos = 0;

    // This doesn't work for really large file offsets.
    // We're limited by Int64 type, not Uint64.
    off_t const res = ::lseek (fd, 0, SEEK_CUR);

    if (res == (off_t) -1) {
        exc_throw (PosixException, errno);
        return Result::Failure;
    }

    if (res < 0) {
        // Unexpected return value from lseek()
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return Result::Failure;
    }

    Uint64 const res64 = (Uint64) res;

    // Uint64 must be able to hold any value of type 'off_t',
    // or at least the file position should not be too large if one
    // wants to get it with NativeFile::tell().
    if ((off_t) res64 != res) {
        exc_throw (InternalException, InternalException::IntegerOverflow);
        return Result::Failure;
    }

    *ret_pos = res64;
    return Result::Success;
}

mt_throws Result
NativeFile::flush ()
{
  // No-op
    return Result::Success;
}

mt_throws Result
NativeFile::sync ()
{
    for (;;) {
	int const res = fsync (fd);
	if (res == -1) {
	    if (errno == EINTR)
		continue;

            exc_throw (PosixException, errno);
            return Result::Failure;
	}

	if (res != 0) {
            exc_throw (InternalException, InternalException::BackendMalfunction);
            return Result::Failure;
	}

	break;
    }

    return Result::Success;
}

mt_throws Result
NativeFile::stat (FileStat * const mt_nonnull ret_stat)
{
    struct stat stat_buf;

    int const res = ::fstat (fd, &stat_buf);
    if (res == -1) {
	exc_throw (PosixException, errno);
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return Result::Failure;
    }

    return posix_statToFileStat (&stat_buf, ret_stat);
}

mt_throws Result
NativeFile::getModificationTime (struct tm * const mt_nonnull ret_tm)
{
    struct stat stat_buf;

    int const res = ::fstat (fd, &stat_buf);
    if (res == -1) {
	exc_throw (PosixException, errno);
	return Result::Failure;
    } else
    if (res != 0) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return Result::Failure;
    }

    splitTime (stat_buf.st_mtime, ret_tm);

    return Result::Success;
}

mt_throws Result
NativeFile::close (bool const /* flush_data */)
{
    /* We should set fd to -1 after closing, and there's no synchronization
     * for it atm => wrong.
     *
    if (!posixClose_nolog (fd, _func)) {
        logE_ (_func, "posixClose() failed: ", exc->toString());
        return Result::Failure;
    }
    */

    return Result::Success;
}

mt_const void
NativeFile::setFd (int const fd)
{
    this->fd = fd;
}

// Note that there may be multiple open() attempts.
mt_const mt_throws Result
NativeFile::open (ConstMemory const filename,
		  Uint32      const open_flags,
		  AccessMode  const access_mode,
                  Uint32      const create_permissions)
{
    int flags = 0;
    switch ((AccessMode::Value) access_mode) {
	case AccessMode::ReadOnly:
	    flags = O_RDONLY;
	    break;
	case AccessMode::WriteOnly:
	    flags = O_WRONLY;
	    break;
	case AccessMode::ReadWrite:
	    flags = O_RDWR;
	    break;
	default:
	    unreachable ();
    }

    if (open_flags && OpenFlags::Create)
	flags |= O_CREAT;

    // TODO Specify behavior for Truncate & O_RDONLY combination.
    if (open_flags & OpenFlags::Truncate)
	flags |= O_TRUNC;

    // TODO Seek to the end of file instead. O_APPEND semantics is too complicated.
    if (open_flags & OpenFlags::Append)
	flags |= O_APPEND;

    StRef<String> const filename_str = st_grab (new (std::nothrow) String (filename));

    for (;;) {
	/* NOTE: man 2 open does not mention EINTR as a possible return
	 * value, while man 3 open _does_. This means that EINTR should
	 * be handled for all invocations of open() in MyCpp (and all
	 * over MyNC). */
	fd = ::open (filename_str->cstr(),
		     // Note that O_DIRECT affects kernel-level caching/buffering
		     // and should not be set here.
		     flags,
                     create_permissions);
	if (fd == -1) {
	    if (errno == EINTR)
		continue;

	    exc_throw (PosixException, errno);
	    exc_push_ (IoException);
	    return Result::Failure;
	}

	break;
    }

    return Result::Success;
}

NativeFile::NativeFile (EmbedContainer * const embed_container,
                        int              const fd)
    : Object (embed_container),
      fd     (fd)
{
}

NativeFile::NativeFile (EmbedContainer * const embed_container)
    : Object (embed_container),
      fd     (-1)
{
}

NativeFile::~NativeFile ()
{
    // For NativeFile used for logging, we should never get here,
    // because that object is never destroyed (deliberate memory leak).
    //
    // Otherwise there would be a problem, because posixClose()
    // calls log*() functions. We don't call posixClose_nolog() here,
    // because we want log messages for non-log NativeFiles.
    //
    posixClose (fd, _func);
}

}

