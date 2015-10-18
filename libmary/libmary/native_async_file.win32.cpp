/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#if 0

#include <libmary/log.h>

#include <libmary/native_async_file.h>


namespace M {

mt_throws AsyncIoResult
NativeAsyncFile::read (OVERLAPPED * const mt_nonnull overlapped,
                       Memory       const mem,
                       Size       * const ret_nread)
{
  // Not implemented
    return AsyncIoResult::Error;
}

mt_throws AsyncIoResult
NativeAsyncFile::write (OVERLAPPED  * const mt_nonnull overlapped,
                        ConstMemory   const mem,
                        Size        * const ret_nwritten)
{
    WSABUF buffers [1];
    buffers [0].buf = (char*) mem.mem();
    buffers [0].len = mem.len();
    return writev (overlapped, buffers, 1, ret_nwritten);
}

mt_throws AsyncIoResult
NativeAsyncFile::writev (OVERLAPPED * const mt_nonnull overlapped,
                         WSABUF     * const mt_nonnull buffers,
                         Count        const buffer_count,
                         Size       * const ret_nwritten)
{
    memset (overlapped, 0, sizeof (OVERLAPPED));

    DWORD bytes_read = 0;
//    bool const res = ReadFile (fd,

  // Win32 doesn't have pure vectored I/O for files.
  // Available WriteFileGather call has severe restrictions
  // on its input data (alignment etc.)
}

mt_throws Result
NativeAsyncFile::seek (FileOffset const offset,
                       SeekOrigin const origin)
{
}

mt_throws Result
NativeAsyncFile::tell (FileSize * const ret_pos)
{
}

mt_throws Result
NativeAsyncFile::sync ()
{
}

mt_throws Result
NativeAsyncFile::close (bool const flush_data)
{
}

NativeAsyncFile::NativeAsyncFile (EmbedContainer * const embed_container)
    : AsyncFile (coderef_container)
{
}

NativeAsyncFile::~NativeAsyncFile ()
{
}

}

#endif

