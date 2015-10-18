/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__FETCH_AGENT__H__
#define MOMENT__FETCH_AGENT__H__


#include <libmary/libmary.h>

#include <moment/channel_options.h>
#include <moment/stream_source.h>
#include <moment/fetch_protocol.h>


namespace Moment {

using namespace M;

class MomentServer;

class FetchAgent : public StreamSource
{
  private:
    StateMutex mutex;

  public:
    struct Frontend
    {
        void (*disconnected) (void *cb_data);
    };

  private:
    class Session : public Object
    {
      public:
        mt_const WeakRef<FetchAgent> weak_fetch_agent;
        mt_const Time connect_begin_time;
        mt_const Ref<FetchConnection> fetch_conn;
        mt_mutex (mutex) bool valid;

        Session (EmbedContainer * const embed_container)
            : Object (embed_container)
        {}
    };

    mt_const Ref<Timers> timers;
    mt_const Ref<DeferredProcessor> deferred_processor;

    mt_const Ref<ChannelOptions> channel_opts;
    mt_const Ref<PlaybackItem> playback_item;
    mt_const Ref<FetchProtocol> fetch_proto;
    mt_const StRef<String> uri;
    mt_const bool do_reconnects;
    mt_const Time reconnect_interval_millisec;

    mt_const Cb<Frontend> frontend;
    mt_const Cb<StreamSource::Frontend> stream_source_frontend;

    DeferredProcessor::Task disconnected_task;
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) Ref<Session> cur_session;
    mt_mutex (mutex) Timers::TimerKey reconnect_timer;

    mt_mutex (mutex) Ref<VideoStream> bound_stream;
    mt_mutex (mutex) Ref<VideoStream> fetch_stream;

    void startNewSession ();

    static void reconnectTimerTick (void *_self);

    mt_unlocks (mutex) void doDisconnected (Session * mt_nonnull session,
                                            bool     report_events);

    static bool disconnectedTask (void *_self);

    mt_iface (FetchConnection::Frontned)
      static FetchConnection::FetchConnectionFrontend const fetch_conn_frontend;

      static void fetchConnDisconnected (DisconnectReason  disconnect_reason,
                                         void             *_session);

      static void fetchConnGotVideo (bool  got_video,
                                     void *_session);
    mt_iface_end

  public:
    void createPipeline ();

    mt_iface (StreamSource)
      void releasePipeline ();

      void getTrafficStats (StreamSource::TrafficStats * mt_nonnull ret_traffic_stats);
      void resetTrafficStats ();
    mt_iface_end

    void init (ServerContext  * mt_nonnull server_ctx,
               ChannelOptions * mt_nonnull channel_opts,
               PlaybackItem   * mt_nonnull playback_item,
               FetchProtocol  * mt_nonnull fetch_proto,
               VideoStream    *stream,
               ConstMemory     uri,
               bool            do_reconnects,
               bool            autostart,
               Time            reconnect_interval_millisec,
               CbDesc<Frontend> const &frontend,
               CbDesc<StreamSource::Frontend> const &stream_source_frontend);

    FetchAgent (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__FETCH_AGENT__H__ */

