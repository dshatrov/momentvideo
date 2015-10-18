/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cstdio>

#include <libmary/log.h>

#include <libmary/native_file.h>


#warning Calling log*() functions may deadlock if the user is inside a logLock()/logUnlock() pair.
// NOTE: Don't call log*() functions here. NativeFile is a part of logging chain.


namespace M {

mt_throws IoResult
NativeFile::read (Memory   const mem,
		  Size   * const ret_nread)
{
    if (ret_nread)
        *ret_nread = 0;

    if (mem.mem() == NULL || mem.len() == 0) {
        logW_ (_func, "empty region");
        return IoResult::Normal;
    }

    DWORD toread;
    if ((Size) (DWORD) mem.len() == mem.len())
        toread = (DWORD) mem.len();
    else
        toread = 0x10000000;

    DWORD nread = 0;
    BOOL const res = ReadFile (fd, mem.mem(), toread, &nread, NULL /* lpOverlapped */);
    if (!res) {
        int const error = GetLastError ();
        if (error == ERROR_HANDLE_EOF)
            return IoResult::Eof;

        exc_throw (Win32Exception, error);
        exc_push_ (IoException);
        return IoResult::Error;
    }

    if (nread > toread) {
	exc_throw (InternalException, InternalException::BackendMalfunction);
	return IoResult::Error;
    }

    if (ret_nread)
        *ret_nread = (Size) nread;

    // TODO Is this correct?
    if (nread == 0)
        return IoResult::Eof;

    return IoResult::Normal;
}

mt_throws Result
NativeFile::write (ConstMemory   const mem,
		   Size        * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    if (mem.mem() == NULL || mem.len() == 0) {
        logW_ (_func, "empty region");
        return Result::Failure;
    }

    DWORD towrite;
    if ((Size) (DWORD) mem.len() == mem.len())
        towrite = (DWORD) mem.len();
    else
        towrite = 0x10000000;

    DWORD nwritten = 0;
    BOOL const res = WriteFile (fd, mem.mem(), mem.len(), &nwritten, NULL /* lpOverlapped */);
    if (!res) {
        exc_throw (Win32Exception, GetLastError ());
        exc_push_ (IoException);
        return Result::Failure;
    }

    if (nwritten > towrite) {
        exc_throw (InternalException, InternalException::BackendMalfunction);
        return Result::Failure;
    }

    if (ret_nwritten)
        *ret_nwritten = (Size) nwritten;

    return Result::Success;
}

mt_throws Result
NativeFile::flush ()
{
  // No-op
    return Result::Success;
}

mt_throws Result
NativeFile::seek (FileOffset const offset,
		  SeekOrigin const origin)
{
    // TODO Make use of lpDistanceToMoveHigh parameter to SetFilePointer().
    if ((FileOffset) (LONG) offset != offset) {
        exc_throw (InternalException, InternalException::BadInput);
        return Result::Failure;
    }

    DWORD whence = 0;
    switch (origin.val()) {
        case SeekOrigin::Beg:
            whence = FILE_BEGIN;
            break;
        case SeekOrigin::Cur:
            whence = FILE_CURRENT;
            break;
        case SeekOrigin::End:
            whence = FILE_END;
            break;
        default:
            unreachable ();
    }

    DWORD const res = SetFilePointer (fd, (LONG) offset, NULL /* lpDistanceToMoveHigh */, whence);
    if (res == INVALID_SET_FILE_POINTER) {
        int const error = GetLastError ();
        if (error != NO_ERROR) {
            exc_throw (Win32Exception, error);
            exc_push_ (IoException);
            return Result::Failure;
        }
    }

    return Result::Success;
}

mt_throws Result
NativeFile::tell (FileSize * const ret_pos)
{
    if (ret_pos)
        *ret_pos = 0;

    // TODO Handle 64bit offsets properly.
    DWORD const res = SetFilePointer (fd,
                                      0 /* offset */,
                                      NULL /* lpDistanceToMoveHigh */,
                                      FILE_CURRENT);
    if (res == INVALID_SET_FILE_POINTER) {
        int const error = GetLastError ();
        if (error != NO_ERROR) {
            exc_throw (Win32Exception, error);
            exc_push_ (IoException);
            return Result::Failure;
        }
    }

    if (ret_pos)
        *ret_pos = (FileSize) res;

    return Result::Success;
}

mt_throws Result
NativeFile::sync ()
{
    BOOL const res = FlushFileBuffers (fd);
    if (!res) {
        exc_throw (Win32Exception, GetLastError ());
        exc_push_ (IoException);
        return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
NativeFile::close (bool const /* flush_data */)
{
    if (fd == INVALID_HANDLE_VALUE) {
        logE_ (_func, "invalid handle");
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return Result::Failure;
    }

    BOOL const res = CloseHandle (fd);
    fd = INVALID_HANDLE_VALUE;
    if (!res) {
        exc_throw (Win32Exception, GetLastError());
        return Result::Failure;
    }

    return Result::Success;
}

mt_throws Result
NativeFile::stat (FileStat * const mt_nonnull ret_stat)
{
    BY_HANDLE_FILE_INFORMATION info;

    BOOL const res = GetFileInformationByHandle (fd, &info);
    if (!res) {
        exc_throw (Win32Exception, GetLastError());
        return Result::Failure;
    }

    ret_stat->size = ((Uint64) info.nFileSizeHigh << 32) | (Uint64) info.nFileSizeLow;
    ret_stat->file_type = FileType::RegularFile;

    return Result::Success;
}

mt_throws Result
NativeFile::getModificationTime (struct tm * const mt_nonnull ret_tm)
{
    memset (ret_tm, 0, sizeof (*ret_tm));

    FILETIME write_time;
    memset (&write_time, 0, sizeof (write_time));
    if (!GetFileTime (fd, NULL /* creation */, NULL /* access */, &write_time)) {
        exc_throw (Win32Exception, GetLastError());
        return Result::Failure;
    }

    SYSTEMTIME systime;
    memset (&systime, 0, sizeof (systime));
    if (!FileTimeToSystemTime (&write_time, &systime)) {
        exc_throw (Win32Exception, GetLastError());
        return Result::Failure;
    }

    ret_tm->tm_sec   = systime.wSecond;
    ret_tm->tm_min   = systime.wMinute;
    ret_tm->tm_hour  = systime.wHour;
    ret_tm->tm_mday  = systime.wDay;
    ret_tm->tm_mon   = systime.wMonth - 1;
    ret_tm->tm_year  = systime.wYear - 1900;
    ret_tm->tm_wday  = systime.wDayOfWeek;
    ret_tm->tm_yday  = 0 /* Unused */;
    ret_tm->tm_isdst = 0 /* Unused */;

    return Result::Success;
}

void
NativeFile::setFd (HANDLE const fd)
{
    this->fd = fd;
}

// Note that there may be multiple open() attempts.
mt_throws Result
NativeFile::open (ConstMemory const filename,
		  Uint32      const open_flags,
		  AccessMode  const access_mode,
                  Uint32      const /* create_permissions */)
{
    DWORD access = 0;
    switch (access_mode.val()) {
        case AccessMode::ReadOnly:
            access = GENERIC_READ;
            break;
        case AccessMode::WriteOnly:
            access = GENERIC_WRITE;
            break;
        case AccessMode::ReadWrite:
            access = GENERIC_READ | GENERIC_WRITE;
            break;
        default:
            unreachable ();
    }

    DWORD creation = OPEN_EXISTING;
    if (open_flags & OpenFlags::Truncate) {
	if (open_flags & OpenFlags::Create)
	    creation = CREATE_ALWAYS;
	else
	    creation = TRUNCATE_EXISTING;
    } else
    if (open_flags & OpenFlags::Create)
	creation = OPEN_ALWAYS;

#if 0
    // TODO Get rid of OpenFlags::Append.
    //     UPD: See OpenFlags::Append handling below.
    if (open_flags & OpenFlags::Append) {
        exc_throw (InternalException, InternalException::BadInput);
        return Result::Failure;
    }
#endif

    StRef<String> const filename_str = st_grab (new (std::nothrow) String (filename));

    fd = CreateFile (filename_str->cstr(),
		     access,
		     FILE_SHARE_READ | FILE_SHARE_WRITE,
		     NULL,
		     creation,
		     FILE_ATTRIBUTE_NORMAL,
		     NULL);
    if (fd == INVALID_HANDLE_VALUE) {
        exc_throw (Win32Exception, GetLastError ());
        exc_push_ (IoException);
        return Result::Failure;
    }

    // TODO Do the same in native_file.linux.cpp
    if (open_flags & OpenFlags::Append) {
        if (!seek (0 /* offset */, SeekOrigin::End)) {
            close ();

            exc_throw_ (IoException);
            return Result::Failure;
        }
    }

    return Result::Success;
}

NativeFile::NativeFile (EmbedContainer * const embed_container,
                        HANDLE           const fd)
    : Object (embed_container),
      fd     (fd)
{
}

NativeFile::NativeFile (EmbedContainer * const embed_container)
    : Object (embed_container),
      fd     (INVALID_HANDLE_VALUE)
{
}

NativeFile::~NativeFile ()
{
    if (fd != INVALID_HANDLE_VALUE) {
        if (!close ())
            logE_ (_func, "close() failed: ", exc->toString());
    }
}

}

