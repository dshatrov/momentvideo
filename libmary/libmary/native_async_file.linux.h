/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__NATIVE_ASYNC_FILE__H__
#define LIBMARY__NATIVE_ASYNC_FILE__H__


#include <libmary/types.h>
#include <libmary/poll_group.h>
#include <libmary/async_file.h>


namespace M {

class NativeAsyncFile : public AsyncFile
{
private:
    int fd;

    Cb<PollGroup::Feedback> feedback;

    void requestInput ()
    {
	if (feedback && feedback->requestInput)
	    feedback.call (feedback->requestInput);
    }

    void requestOutput ()
    {
	if (feedback && feedback->requestOutput)
	    feedback.call (feedback->requestOutput);
    }

  mt_iface (PollGroup::Pollable)
    static PollGroup::Pollable const pollable;

    static void pollable_processEvents (Uint32  event_flags,
                                        void   *_self);

    static void pollable_setFeedback (Cb<PollGroup::Feedback> const &feedback,
                                      void *_self);

    static int pollable_getFd (void *_self);
  mt_iface_end

public:
  mt_iface (AsyncFile)
    mt_iface (AsyncInputStream)
      mt_throws AsyncIoResult read (Memory  mem,
                                    Size   *ret_nread);
    mt_iface_end

    mt_iface (AsyncOutputStream)
      mt_throws AsyncIoResult write (ConstMemory  mem,
                                     Size        *ret_nwritten);
    mt_iface_end

    mt_throws Result seek (FileOffset offset,
                             SeekOrigin origin);

    mt_throws Result tell (FileSize *ret_pos);

    mt_throws Result sync ();

    mt_throws Result close (bool flush_data = true);
  mt_iface_end

    int getFd () { return fd; }

    void setFd (int fd);

    mt_throws Result open (ConstMemory    filename,
                           Uint32         open_flags,
                           FileAccessMode access_mode);

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    NativeAsyncFile (EmbedContainer *embed_container,
                     int             fd = -1);

    ~NativeAsyncFile ();
};

}


#endif /* LIBMARY__NATIVE_ASYNC_FILE__H__ */

