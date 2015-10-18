/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__MEDIA_VIEWER__H__
#define MOMENT_NVR__MEDIA_VIEWER__H__


#include <moment-nvr/media_reader.h>


namespace MomentNvr {

class MediaViewer : public Object
{
  private:
    StateMutex mutex;

    class Session : public Object
    {
      public:
        StateMutex session_mutex;

        mt_const WeakRef<MediaViewer> weak_media_viewer;
        mt_const Ref<VideoStream> stream;

        MediaReader media_reader;

        mt_mutex (session_mutex) bool watching;
        mt_mutex (session_mutex) StRef<String> stream_name;

        mt_mutex (session_mutex) bool started;
        mt_mutex (session_mutex) GenericInformer::SubscriptionKey stream_state_sbn;

        // TODO Use sendMoreData as a synchronization domain
        //      Currently, synchronization is not fully correct for Session.
        mt_mutex (session_mutex) bool first_frame;
        mt_mutex (session_mutex) Time first_frame_ts;
        mt_mutex (session_mutex) Time first_frame_srv_time;

        AtomicInt send_blocked;

        mt_mutex (session_mutex) Timers::TimerKey send_timer;

        Session (EmbedContainer * const embed_container)
            : Object       (embed_container),
              media_reader (this /* embed_container */)
        {}
    };

    mt_const Ref<PagePool> page_pool;
    mt_const Ref<Timers> timers;

    mt_const StRef<Vfs> vfs;

    static MediaReader::ReadFrameResult endFrame (Session      * mt_nonnull session,
                                                  MediaMessage * mt_nonnull msg);

    mt_iface (MediaReader::ReadFrameBackend)
      static MediaReader::ReadFrameBackend const read_frame_backend;

      static MediaReader::ReadFrameResult audioFrame (AudioMessage * mt_nonnull msg,
                                                      void         *_session);

      static MediaReader::ReadFrameResult videoFrame (VideoMessage * mt_nonnull msg,
                                                      void         *_session);
    mt_iface_end

    void sendMoreData (Session * mt_nonnull session);

    static void sendTimerTick (void *_session);

    mt_iface (VideoStream::StateEvents)
      static VideoStream::StateEvents const stream_state_events;

      static void streamNumWatchersChanged (Count  num_watchers,
                                            void  *_session);
    mt_iface_end

    static mt_mutex (Session::session_mutex) void setSendState (Session           * mt_nonnull session,
                                                                Sender::SendState  send_state);

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void senderStateChanged (Sender::SendState  send_state,
                                      void              *_session);
    mt_iface_end

    mt_iface (MomentServer::ClientHandler)
      static MomentServer::ClientHandler const client_handler;

      static void rtmpClientConnected (MomentServer::ClientSession *client_session,
                                       ConstMemory  app_name,
                                       ConstMemory  full_app_name,
                                       void        *_self);
    mt_iface_end

    mt_iface (MomentServer::ClientSession::Events)
      static MomentServer::ClientSession::Events const client_session_events;

      static void rtmpClientDisconnected (void *_session);
    mt_iface_end

    struct StreamParams
    {
        Time start_unixtime_sec;

        StreamParams ()
            : start_unixtime_sec (0)
        {}
    };

    static void parseStreamParams_paramCallback (ConstMemory  name,
                                                 ConstMemory  value,
                                                 void        *_stream_params);

    static void parseStreamParams (ConstMemory   stream_name_with_params,
                                   StreamParams * mt_nonnull stream_params);

    mt_iface (MomentServer::ClientSession::Backend)
      static MomentServer::ClientSession::Backend const client_session_backend;

      static bool rtmpStartWatching (ConstMemory       stream_name,
                                     ConstMemory       stream_name_with_params,
                                     IpAddress         client_addr,
                                     CbDesc<MomentServer::StartWatchingCallback> const &cb,
                                     Ref<VideoStream> * mt_nonnull ret_stream,
                                     void             *_session);

      static bool rtmpStartStreaming (ConstMemory    stream_name,
                                      IpAddress      client_addr,
                                      VideoStream   * mt_nonnull stream,
                                      RecordingMode  rec_mode,
                                      CbDesc<MomentServer::StartStreamingCallback> const &cb,
                                      Result        * mt_nonnull ret_res,
                                      void          *_session);
    mt_iface_end

  public:
    void init (MomentServer * mt_nonnull moment,
               Vfs          * mt_nonnull vfs);

    MediaViewer (EmbedContainer *embed_container);
};

}


#endif /* MOMENT_NVR__MEDIA_VIEWER__H__ */

