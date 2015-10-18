/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#if 0

#ifndef LIBMARY__NATIVE_ASYNC_FILE__WIN32__H__
#define LIBMARY__NATIVE_ASYNC_FILE__WIN32__H__


#include <libmary/async_file.h>
#include <libmary/poll_group.h>


namespace M {

class NativeAsyncFile : public AsyncFile,
                        public virtual DependentCodeReferenced
{
private:
    StateMutex mutex;

    mt_const HANDLE fd;

    mt_mutex (mutex) Uint64 cur_write_offset;

public:
  mt_iface (AsyncFile)
    mt_iface (Connection)
      mt_iface (AsyncInputStream)
        mt_throws AsyncIoResult read (OVERLAPPED * mt_nonnull overlapped,
                                      Memory      mem,
                                      Size       *ret_nread);
      mt_iface_end

      mt_iface (AsyncOutputStream)
        mt_throws AsyncIoResult write (OVERLAPPED  * mt_nonnull overlapped,
                                       ConstMemory  mem,
                                       Size        *ret_nwritten);

        mt_throws AsyncIoResult writev (OVERLAPPED * mt_nonnull overlapped,
                                        WSABUF     * mt_nonnull buffers,
                                        Count       buffer_count,
                                        Size       *ret_nwritten);
      mt_iface_end
    mt_iface_end

    mt_throws Result seek (FileOffset offset,
                           SeekOrigin origin);

    mt_throws Result tell (FileSize *ret_pos);

    mt_throws Result sync ();

    mt_throws Result close (bool flush_data = true);
  mt_iface_end

    mt_throws Result open (ConstMemory    filename,
                           Uint32         open_flags,
                           FileAccessMode access_mode);

//    CbDesc<PollGroup::Pollable> getPollable ()
//        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

     NativeAsyncFile (EmbedContainer *embed_container);
    ~NativeAsyncFile ();
};

}


#endif /* LIBMARY__NATIVE_ASYNC_FILE__WIN32__H__ */

#endif

