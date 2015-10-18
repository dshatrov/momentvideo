/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LINE_FD_PIPE__H__
#define LIBMARY__LINE_FD_PIPE__H__


#include <libmary/poll_group.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/native_async_file.linux.h>
  #include <libmary/connection_receiver.generic.h>
#else
  #include <libmary/native_async_file.h>
  #include <libmary/connection_receiver.h>
#endif
#include <libmary/line_server.h>


namespace M {

class LineFdPipe : public Object
{
private:
    StateMutex mutex;

public:
    typedef LineServer::Frontend Frontend;

private:
    mt_const Cb<Frontend> frontend;
    mt_const Ref<PollGroup> poll_group;

    mt_const bool line_file_opened;

    mt_mutex (mutex) PollGroup::PollableKey pollable_key;

    Embed<NativeAsyncFile>    line_file;
    Embed<ConnectionReceiver> line_receiver;
    Embed<LineServer>         line_server;

  mt_iface (LineServer::Frontend)
    static LineServer::Frontend const line_frontend;

    static void line (ConstMemory  line,
                      void        *_pipe_session);

    static void closed (void *_pipe_session);
  mt_iface_end

    void doRelease ();

public:
    mt_const mt_throws Result init (int                     fd,
                                    CbDesc<Frontend> const &frontend,
                                    PollGroup              * mt_nonnull poll_group,
                                    DeferredProcessor      * mt_nonnull deferred_processor,
                                    Size                    max_line_len = 4096);

     LineFdPipe (EmbedContainer *embed_container);
    ~LineFdPipe ();
};

}


#endif /* LIBMARY__LINE_FD_PIPE__H__ */

