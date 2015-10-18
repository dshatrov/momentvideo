/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__AV_RECORDER__H__
#define MOMENT__AV_RECORDER__H__


#include <libmary/libmary.h>

#include <moment/storage.h>
#include <moment/stream.h>
#include <moment/av_muxer.h>


namespace Moment {

using namespace M;

class AvRecorder : public Object
{
  private:
    Mutex mutex;

  public:
    struct Frontend {
        void (*error) (Exception *exc_,
                       void      *cb_data);
    };

  private:
    // Tickets help to distinguish asynchronous messages from different streams.
    // This allows to ignore messages from the old stream when switching streams.
    class StreamTicket : public Referenced
    {
      public:
        AvRecorder * const av_recorder;

        StreamTicket (AvRecorder * const av_recorder)
            : av_recorder (av_recorder)
        {}
    };

    class Recording : public Object
    {
      public:
        StateMutex mutex;

        WeakRef<AvRecorder> weak_av_recorder;

        Connection *conn;
        mt_mutex (mutex) Ref<Storage::StorageFile> storage_file;

        Embed<DeferredConnectionSender> sender;

        Recording (EmbedContainer * const embed_container)
            : Object (embed_container),
              sender (this /* embed_container */)
        {}

        ~Recording () {}
    };

    mt_mutex (mutex) Ref<VideoStream> video_stream;
    mt_mutex (mutex) Ref<StreamTicket> cur_stream_ticket;

    mt_const Cb<Frontend> frontend;

    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const Ref<Storage> storage;

    // Muxer operations should be synchronized with 'mutex'.
    mt_const AvMuxer *muxer;

    mt_const Uint64 recording_limit;

    mt_mutex (mutex) Ref<Recording> recording;
    mt_mutex (mutex) bool paused;

    mt_mutex (mutex) bool got_first_frame;
    mt_mutex (mutex) Time first_frame_time;

    mt_mutex (mutex) Uint64 total_bytes_recorded;

    mt_iface (VideoStream::FrameSaver::FrameHandler)
      static FrameSaver::FrameHandler const saved_frame_handler;

      static Result savedAudioFrame (AudioMessage * mt_nonnull audio_msg,
                                     void         *_self);

      static Result savedVideoFrame (VideoMessage * mt_nonnull video_msg,
                                     void         *_self);
    mt_iface_end

    mt_mutex (mutex) void muxInitialMessages ();

    mt_mutex (mutex) void doStop ();

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void senderStateChanged (SenderState  sender_state,
                                      void        *_recording);

      static void senderClosed (Exception *exc_,
                                void      *_recording);
    mt_iface_end

    mt_iface (VideoStream::Events)
      static VideoStream::Events const stream_events;

      static void streamAudioMessage (AudioMessage * mt_nonnull msg,
                                      void         *_stream_ticket);

      static void streamVideoMessage (VideoMessage * mt_nonnull msg,
                                      void         *_stream_ticket);

      static void streamClosed (void *_stream_ticket);
    mt_iface_end

  public:
    void start  (ConstMemory filename,
                 Uint32 create_permissions = LIBMARY__DEFAULT_CREATE_PERMISSIONS);

    void pause  ();
    void resume ();
    void stop   ();

    void setVideoStream (VideoStream *stream);

    mt_const void init (CbDesc<Frontend> const &frontend,
                        ServerThreadContext * mt_nonnull thread_ctx,
                        Storage             * mt_nonnull storage,
                        AvMuxer             * mt_nonnull muxer,
                        Uint64               recording_limit);

     AvRecorder (EmbedContainer *embed_container);
    ~AvRecorder ();
};

}


#endif /* MOMENT__AV_RECORDER__H__ */

