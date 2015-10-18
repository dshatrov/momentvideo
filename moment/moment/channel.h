/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__CHANNEL__H__
#define MOMENT__CHANNEL__H__


#include <moment/channel_options.h>
#include <moment/playback.h>
#include <moment/source_manager.h>
#include <moment/stream_manager.h>


namespace Moment {

class MomentServer;

class Channel : public Object
{
  private:
    StateMutex mutex;


  // _____________________________ event_informer ______________________________

  public:
    struct ChannelEvents
    {
        void (*startItem)      (MediaStream *new_stream,
                                MediaStream *old_stream,
                                void        *cb_data);

//#warning TODO Merge stopItem into startItem
// ^ In fact, stopItem() is unused and can be removed.
        void (*stopItem)       (MediaStream *new_stream,
                                MediaStream *old_stream,
                                void        *cb_data);

// TODO newVideoStream and startItem logically overlap. There's no reason for this separation.
//      It feels like the only callback I may need is "mediaStreamChanged".
        void (*newVideoStream) (MediaStream *new_stream,
                                MediaStream *old_stream,
                                void        *cb_data);

        void (*destroyed)      (void *cb_data);
    };

  private:
    Informer_<ChannelEvents> event_informer;

    struct EventData
    {
        MediaStream *new_stream;
        MediaStream *old_stream;

        EventData (MediaStream * const new_stream,
                   MediaStream * const old_stream)
            : new_stream (new_stream),
              old_stream (old_stream)
        {}
    };

    static void informStartItem (ChannelEvents * const events,
                                 void          * const cb_data,
                                 void          * const _data)
    {
        if (events->startItem) {
            EventData * const data = static_cast <EventData*> (_data);
            events->startItem (data->new_stream, data->old_stream, cb_data);
        }
    }

    static void informStopItem (ChannelEvents * const events,
                                void          * const cb_data,
                                void          * const _data)
    {
        if (events->stopItem) {
            EventData * const data = static_cast <EventData*> (_data);
            events->stopItem (data->new_stream, data->old_stream, cb_data);
        }
    }

    static void informNewVideoStream (ChannelEvents * const events,
                                      void          * const cb_data,
                                      void          * const _data)
    {
        if (events->newVideoStream) {
            EventData * const data = static_cast <EventData*> (_data);
            events->newVideoStream (data->new_stream, data->old_stream, cb_data);
        }
    }

    static void informDestroyed (ChannelEvents * const events,
                                 void          * const cb_data,
                                 void          * const /* inform_data */)
        { if (events->destroyed) events->destroyed (cb_data); }

    void fireStartItem (MediaStream * const new_stream,
                        MediaStream * const old_stream)
    {
        EventData data (new_stream, old_stream);
        event_informer.informAll (informStartItem, &data);
    }

    void fireStopItem (MediaStream * const new_stream,
                       MediaStream * const old_stream)
    {
        EventData data (new_stream, old_stream);
        event_informer.informAll (informStopItem, &data);
    }

    void fireNewVideoStream (MediaStream * const new_stream,
                             MediaStream * const old_stream)
    {
        EventData data (new_stream, old_stream);
        event_informer.informAll (informNewVideoStream, &data);
    }

    mt_unlocks_locks (mutex) void fireDestroyed_locked ()
        { event_informer.informAll_locked (informDestroyed, NULL); }

  public:
    Informer_<ChannelEvents>* getEventInformer () { return &event_informer; }

    void channelLock   () { mutex.lock   (); }
    void channelUnlock () { mutex.unlock (); }

    mt_mutex (mutex) MediaStream* getMediaStream_locked () const { return stream; }
    mt_mutex (mutex) bool         isDestroyed_locked    () const { return destroyed; }


  // ________________________________ playback _________________________________

  private:
    Embed<Playback> playback;

    mt_iface (Playback::Frontend)
      static Playback::Frontend playback_frontend;

      static void startPlaybackItem (Playlist::Item          *playlist_item,
                                     Time                     seek,
                                     Playback::AdvanceTicket *advance_ticket,
                                     void                    *_self);
    mt_iface_end

  public:
    Playback* getPlayback () { return playback; }

  // ___________________________________________________________________________


  private:
    mt_const Ref<ChannelOptions> channel_opts;

    mt_const WeakRef<SourceManager> weak_source_manager;
    mt_const WeakRef<StreamManager> weak_stream_manager;

    mt_const Ref<Timers>            timers;
    mt_const Ref<DeferredProcessor> deferred_processor;
    mt_const Ref<PagePool>          page_pool;

    mt_mutex (mutex) Ref<PlaybackItem> cur_item;

    struct StreamData : public Referenced
    {
        Channel * const channel;

        mt_mutex (mutex) Count num_watchers;

        StreamData (Channel * const channel)
            : channel (channel),
              num_watchers (0)
        {}
    };

    struct SourceData : public Referenced
    {
        Channel * const channel;

        mt_mutex (mutex) bool source_closed;

        SourceData (Channel * const channel)
            : channel (channel),
              source_closed (false)
        {}
    };

    mt_mutex (mutex) Ref<StreamSource> stream_source;
    mt_mutex (mutex) bool source_gap;

    // Internal tickets
    mt_mutex (mutex) Ref<StreamData> cur_stream_data;
    mt_mutex (mutex) Ref<SourceData> cur_source_data;

    mt_mutex (mutex) Ref<Stream> stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey stream_state_events_sbn;
    mt_mutex (mutex) StreamManager::StreamKey stream_key;

    mt_mutex (mutex) bool destroyed;

    mt_mutex (mutex) Ref<Playback::AdvanceTicket> advance_ticket;

    mt_mutex (mutex) bool got_video;

    mt_mutex (mutex) Time stream_start_time;


  // ____________________________ connect on demand ____________________________

    mt_mutex (mutex) Timers::TimerKey connect_on_demand_timer;

    mt_iface (Stream::StateEvents)
        static Stream::StateEvents const stream_state_events;

        static void numWatchersChanged (Count  num_watchers,
                                        void  *_stream_data);
    mt_iface_end

    static void connectOnDemandTimerTick (void *_stream_data);

    void beginConnectOnDemand ();

  // ___________________________________________________________________________


    void setStreamParameters (Stream * mt_nonnull stream);

    mt_mutex (mutex) void createStream (Time initial_seek);

    mt_mutex (mutex) void closeStream (Ref<Stream> * mt_nonnull ret_old_stream);

    mt_mutex (mutex) void stopConnectOnDemand ();
    mt_mutex (mutex) void releaseSource ();
    mt_mutex (mutex) void releaseStream (Ref<Stream> * mt_nonnull ret_old_stream);
    mt_mutex (mutex) void replaceStream (Ref<Stream> * mt_nonnull ret_old_stream);

    mt_unlocks (mutex) void doRestartStream ();

    void doSourceEos (SourceData * mt_nonnull source_data);

    mt_iface (StreamSource::Frontend)
      static StreamSource::Frontend stream_source_frontend;

      static void sourceError (void *_source_data);
      static void sourceEos   (void *_source_data);
      static void noVideo     (void *_source_data);
      static void gotVideo    (void *_source_data);
    mt_iface_end

    void doDestroy (bool from_dtor);

  public:
    void restartStream  ();

    // TODO Unused?
    void destroy ();

    bool isSourceOnline ();


  // ______________________________ traffic stats ______________________________

  private:
    mt_mutex (mutex) Uint64 rx_bytes_accum;
    mt_mutex (mutex) Uint64 rx_audio_bytes_accum;
    mt_mutex (mutex) Uint64 rx_video_bytes_accum;

  public:
    struct TrafficStats
    {
        Uint64 rx_bytes;
        Uint64 rx_audio_bytes;
        Uint64 rx_video_bytes;
        Time   time_elapsed;
    };

    void getTrafficStats (TrafficStats *ret_traffic_stats);

    void resetTrafficStats ();

  // ___________________________________________________________________________


  public:
    ChannelOptions* getChannelOptions () { return channel_opts; }

    mt_const void init (MomentServer   * mt_nonnull moment,
                        ChannelOptions * mt_nonnull channel_opts);

     Channel (EmbedContainer *embed_container);
    ~Channel ();
};

}


#endif /* MOMENT__CHANNEL__H__ */

