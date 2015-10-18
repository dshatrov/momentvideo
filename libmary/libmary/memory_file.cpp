/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/memory_file.h>


namespace M {

mt_throws IoResult
MemoryFile::read (Memory   const mem,
                  Size   * const ret_nread)
{
    if (ret_nread)
        *ret_nread = 0;

    if (array_mem.mem() == NULL) {
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return IoResult::Error;
    }

    if (pos >= array_mem.len())
        return IoResult::Eof;

    Size lim;

    if (array_mem.len() - pos < mem.len())
        lim = array_mem.len() - pos;
    else
        lim = mem.len();

    memcpy (mem.mem(), array_mem.mem() + pos, lim);
    pos += lim;

    if (ret_nread)
        *ret_nread = lim;

    return IoResult::Normal;
}

mt_throws Result
MemoryFile::write (ConstMemory   const mem,
                   Size        * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    if (array_mem.mem() == NULL) {
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return Result::Failure;
    }

    if (pos >= array_mem.len()) {
        // TODO Is this correct? This mimcs NativeFile's behavior,
        //      which may be wrong, too.
        return Result::Success;
    }

    Size lim;
    lim = array_mem.len() - pos;
    if (lim > mem.len())
        lim = mem.len();

    memcpy (array_mem.mem() + pos, mem.mem(), lim);

    pos += lim;

    if (ret_nwritten)
        *ret_nwritten = lim;

    return Result::Success;
}

mt_throws Result
MemoryFile::flush ()
{
  // No-op
    return Result::Success;
}

mt_throws Result
MemoryFile::seek (FileOffset const offset,
                  SeekOrigin const origin)
{
    if (array_mem.mem() == NULL) {
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return Result::Failure;
    }

    if (origin == SeekOrigin::Beg) {
        if (offset < 0 ||
            (Size) offset > array_mem.len())
        {
            exc_throw (InternalException, InternalException::OutOfBounds);
            return Result::Failure;
        }

        pos = (Size) offset;
    } else
    if (origin == SeekOrigin::Cur) {
        if (offset > 0) {
            if ((Size) offset > array_mem.len() - pos) {
                exc_throw (InternalException, InternalException::OutOfBounds);
                return Result::Failure;
            }

            pos += (Size) offset;
        } else
        if (offset < 0) {
            if ((Size) -offset > pos) {
                exc_throw (InternalException, InternalException::OutOfBounds);
                return Result::Failure;
            }

            pos -= (Size) offset;
        }
    } else
    if (origin == SeekOrigin::End) {
        if (offset > 0 ||
            (Size) -offset > array_mem.len())
        {
            exc_throw (InternalException, InternalException::OutOfBounds);
            return Result::Failure;
        }

        pos = array_mem.len() - (Size) -offset;
    } else {
        unreachable ();
    }

    return Result::Success;
}

mt_throws Result
MemoryFile::tell (FileSize * const ret_pos)
{
    if (ret_pos)
        *ret_pos = pos;

    return Result::Success;
}

mt_throws Result
MemoryFile::sync ()
{
  // No-op
    return Result::Success;
}

mt_throws Result
MemoryFile::close (bool const /* flush_data */)
{
  // No-op
    return Result::Success;
}

MemoryFile::MemoryFile (Memory const mem)
    : array_mem (mem),
      pos (0)
{
}

}

