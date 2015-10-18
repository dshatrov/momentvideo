/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LINE_PIPE__H__
#define LIBMARY__LINE_PIPE__H__


#include <libmary/timers.h>
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

class LinePipe : public Object
{
private:
    StateMutex mutex;

public:
    typedef LineServer::Frontend Frontend;

private:
    mt_const StRef<String> filename;
    mt_const Time reopen_timeout_millisec;
    mt_const Size max_line_len;

    mt_const Cb<Frontend> frontend;

    mt_const Ref<PollGroup> poll_group;
    mt_const Ref<DeferredProcessor> deferred_processor;
    mt_const Ref<Timers> timers;

    class PipeSession : public Object
    {
      public:
        mt_const WeakRef<LinePipe> weak_line_pipe;

        mt_const PollGroup::PollableKey pollable_key;

        Embed<NativeAsyncFile>    line_file;
        Embed<ConnectionReceiver> line_receiver;
        Embed<LineServer>         line_server;

        PipeSession (EmbedContainer * const embed_container)
            : Object        (embed_container),
              line_file     (this /* embed_container */),
              line_receiver (this /* embed_container */),
              line_server   (this /* embed_container */)
        {}
    };

    mt_mutex (mutex) Ref<PipeSession> pipe_session;

    mt_mutex (mutex) Timers::TimerKey reopen_timer;
    mt_mutex (mutex) StRef<String> prv_error_str;

  mt_iface (LineServer::Frontend)
    static LineServer::Frontend const line_frontend;

    static void line (ConstMemory  line,
                      void        *_pipe_session);

    static void closed (void *_pipe_session);
  mt_iface_end

    static void reopenTimerTick (void *_self);

    mt_mutex (mutex) void releasePipeSession ();

    mt_mutex (mutex) mt_throws Result openPipeSession ();

public:
    mt_const mt_throws Result init (ConstMemory             filename,
                                    CbDesc<Frontend> const &frontend,
                                    PollGroup              * mt_nonnull poll_group,
                                    DeferredProcessor      * mt_nonnull deferred_processor,
                                    Timers                 *timers,
                                    Time                    reopen_timeout_millisec,
                                    Size                    max_line_len = 4096);

     LinePipe (EmbedContainer *embed_container);
    ~LinePipe ();
};

}


#endif /* LIBMARY__LINE_PIPE__H__ */

