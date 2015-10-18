/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__MEDIA_STREAM__H__
#define LIBMOMENT__MEDIA_STREAM__H__


#include <moment/media_source.h>
#include <moment/media_message.h>
#include <moment/domain_options.h>
#include <moment/frame_saver.h>
#include <moment/amf_decoder.h>


namespace Moment {

class StreamParameters : public Referenced
{
  private:
    StringHash< StRef<String> > params;

  public:
    bool hasParam (ConstMemory const name)
    {
        StringHash< StRef<String> >::EntryKey const key = params.lookup (name);
        return (bool) key;
    }

    ConstMemory getParam (ConstMemory   const name,
                          bool        * const ret_has_param = NULL)
    {
        StringHash< StRef<String> >::EntryKey const key = params.lookup (name);
        if (!key) {
            if (ret_has_param)
                *ret_has_param = false;

            return ConstMemory ();
        }

        if (ret_has_param)
            *ret_has_param = true;

        return key.getDataPtr()->ptr()->mem();
    }

    void setParam (ConstMemory const name,
                   ConstMemory const value)
        { params.add (name, st_grab (new (std::nothrow) String (value))); }
};

class RtmpConnection;

class VideoStream : public MediaSource
{
  private:
    StateMutex mutex;

    DeferredProcessor::Task report_num_watchers_task;
    DeferredProcessor::Task report_pending_events_task;
    DeferredProcessor::Task close_task;
    DeferredProcessor::Registration deferred_reg;

  public:
    class MomentServerData
    {
      public:
        Ref<Referenced> stream_info;
        mt_mutex (StreamManager::mutex) Count use_count;

        MomentServerData ()
            : use_count (0)
        {}
    };

    // Should be used by MomentServer only.
    mt_mutex (MomentServer::mutex) MomentServerData moment_data;

    static bool msgHasTimestamp (MediaMessage * const msg,
                                 bool           const is_audio_msg)
    {
        if (is_audio_msg) {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);
            return audio_msg->frame_type.hasTimestamp ();
        }

        VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
        return video_msg->frame_type.hasTimestamp ();
    }

    struct Events
    {
        void (*audioMessage) (AudioMessage * mt_nonnull audio_msg,
                              void         *cb_data);

        void (*videoMessage) (VideoMessage * mt_nonnull video_msg,
                              void         *cb_data);

        void (*rtmpDataMessage) (MediaMessage * mt_nonnull msg,
                                 void         *cb_data);

        void (*rtmpCommandMessage) (RtmpConnection * mt_nonnull conn,
                                    MediaMessage   * mt_nonnull msg,
                                    ConstMemory     method_name,
                                    AmfDecoder     * mt_nonnull amf_decoder,
                                    void           *cb_data);

//        void (*playbackStart) (void *cb_data);

//        void (*playbackStop) (void *cb_data);

        void (*closed) (void *cb_data);

        void (*numWatchersChanged) (Count  num_watchers,
                                    void  *cb_data);

        void (*mediaDesc) (MediaDesc_Audio *audio_desc,
                           bool             set_audio,
                           MediaDesc_Video *video_desc,
                           bool             set_video,
                           void            *cb_data);
    };

    struct StateEvents
    {
        void (*closed) (void *cb_data);

        void (*numWatchersChanged) (Count  num_watchers,
                                    void  *cb_data);

        void (*publishingStatusChanged) (bool  publishing_in_progress,
                                         void *cb_data);

        void (*mediaDesc) (MediaDesc_Audio *audio_desc,
                           bool             set_audio,
                           MediaDesc_Video *video_desc,
                           bool             set_video,
                           void            *cb_data);
    };

    struct StreamFeedback
    {
//#warning TODO StreamFeedback
        void (*feedbackPausePlayback)  (void *cb_data);
        void (*feedbackResumePlayback) (void *cb_data);
    };

  private:
    class PendingEventList_name;

    class PendingEvent : public IntrusiveListElement<PendingEventList_name> {
      public:
        enum Type {
            t_MediaDesc,
            t_Audio,
            t_Video,
            t_Data,
            t_Event_BeginPublishing,
            t_Event_EndPublishing
        };
      private:
        Type const type;
      public:
        Type getType() const { return type; }
        PendingEvent (Type const type) : type (type) {}
        virtual ~PendingEvent () {}
    };

    typedef IntrusiveList<PendingEvent, PendingEventList_name> PendingEventList;

    struct PendingMediaDesc : public PendingEvent
    {
        Ref<MediaDesc_Audio> audio_desc;
        bool set_audio;

        Ref<MediaDesc_Video> video_desc;
        bool set_video;

        PendingMediaDesc (MediaDesc_Audio * const audio_desc,
                          bool              const set_audio,
                          MediaDesc_Video * const video_desc,
                          bool              const set_video)
            : PendingEvent (t_MediaDesc),
              audio_desc   (audio_desc),
              set_audio    (set_audio),
              video_desc   (video_desc),
              set_video    (set_video)
        {}
    };

    struct PendingAudioFrame : public PendingEvent
    {
        AudioMessage audio_msg;

        PendingAudioFrame (AudioMessage * const mt_nonnull msg)
            : PendingEvent (t_Audio)
        {
            audio_msg = *msg;
            msg->seize ();
        }

        ~PendingAudioFrame () { audio_msg.release (); }
    };

    struct PendingVideoFrame : public PendingEvent
    {
        VideoMessage video_msg;

        PendingVideoFrame (VideoMessage * const mt_nonnull msg)
            : PendingEvent (t_Video)
        {
            video_msg = *msg;
            msg->seize ();
        }

        ~PendingVideoFrame () { video_msg.release (); }
    };

    struct PendingDataFrame : public PendingEvent
    {
        MediaMessage data_msg;

        PendingDataFrame (MediaMessage * const mt_nonnull msg)
            : PendingEvent (t_Data)
        {
            data_msg = *msg;
            msg->seize ();
        }

        ~PendingDataFrame () { data_msg.release (); }
    };

    mt_mutex (mutex) FrameSaver frame_saver;

    Informer_<Events> event_informer;
    Informer_<StateEvents> state_informer;

    static void informMediaDesc (Events *events,
                                 void   *cb_data,
                                 void   *inform_data);

    static void stateInformMediaDesc (StateEvents *state_events,
                                      void        *cb_data,
                                      void        *inform_data);

    static void informAudioMessage (Events *events,
                                    void   *cb_data,
                                    void   *inform_data);

    static void informVideoMessage (Events *events,
                                    void   *cb_data,
                                    void   *inform_data);

    static void informRtmpDataMessage (Events *events,
                                       void   *cb_data,
                                       void   *inform_data);

    static void informRtmpCommandMessage (Events *events,
                                          void   *cb_data,
                                          void   *inform_data);

    static void informClosed (Events *events,
                              void   *cb_data,
                              void   *inform_data);

    static void stateInformClosed (StateEvents *state_events,
                                   void        *cb_data,
                                   void        *inform_data);

//    Informer_<StreamFeedback> feedback_informer;

    static void informFeedbackPausePlayback (StreamFeedback *feedback,
                                             void           *cb_data,
                                             void           *inform_data);

    static void informFeedbackResumePlayback (StreamFeedback *feedback,
                                              void           *cb_data,
                                              void           *inform_data);

  public:
    // Returned FrameSaver must be synchronized manually with VideoStream::lock/unlock().
    FrameSaver* getFrameSaver () { return &frame_saver; }

    // It is guaranteed that the informer can be controlled with
    // VideoStream::lock/unlock() methods.
    Informer_<Events>*      getEventInformer ()      { return &event_informer; }
    Informer_<StateEvents>* getStateEventInformer () { return &state_informer; }

  private:
    mt_async mt_unlocks_locks (mutex) void reportPendingEvents ();

    static bool reportPendingEventsTask (void *_self);

    mt_mutex (mutex) void reportPendingEvents_deferred ();

  public:
    mt_async mt_unlocks_locks (mutex) void fireMediaDesc_locked (MediaDesc_Audio *audio_desc,
                                                                 bool             set_audio,
                                                                 MediaDesc_Video *video_desc,
                                                                 bool             set_video);

    mt_async void fireMediaDesc (MediaDesc_Audio * const audio_desc,
                                 bool              const set_audio,
                                 MediaDesc_Video * const video_desc,
                                 bool              const set_video)
    {
        mutex.lock ();
        mt_async mt_unlocks_locks (mutex) fireMediaDesc_locked (
                audio_desc, set_audio, video_desc, set_video);
        mutex.unlock ();
    }

    mt_async mt_unlocks_locks (mutex) void fireAudioMessage_locked (AudioMessage * mt_nonnull audio_msg);

    mt_async void fireAudioMessage (AudioMessage * const mt_nonnull audio_msg)
    {
        mutex.lock ();
        mt_async mt_unlocks_locks (mutex) fireAudioMessage_locked (audio_msg);
        mutex.unlock ();
    }

    mt_async mt_unlocks_locks (mutex) void fireVideoMessage_locked (VideoMessage * mt_nonnull video_msg);

    mt_async void fireVideoMessage (VideoMessage * const mt_nonnull video_msg)
    {
        mutex.lock ();
        mt_async mt_unlocks_locks (mutex) fireVideoMessage_locked (video_msg);
        mutex.unlock ();
    }

    mt_async mt_unlocks_locks (mutex) void fireRtmpDataMessage_locked (MediaMessage * mt_nonnull msg);

    mt_async void fireRtmpDataMessage (MediaMessage * const mt_nonnull msg)
    {
        mutex.lock ();
        mt_async mt_unlocks_locks (mutex) fireRtmpDataMessage_locked (msg);
        mutex.unlock ();
    }

    mt_async mt_unlocks_locks (mutex) void fireRtmpCommandMessage_locked (RtmpConnection * mt_nonnull conn,
                                                                          MediaMessage   * mt_nonnull msg,
                                                                          ConstMemory     method_name,
                                                                          AmfDecoder     * mt_nonnull amf_decoder);

    mt_async void fireRtmpCommandMessage (RtmpConnection * const mt_nonnull conn,
                                          MediaMessage   * const mt_nonnull msg,
                                          ConstMemory      const method_name,
                                          AmfDecoder     * const mt_nonnull amf_decoder)
    {
        mutex.lock ();
        mt_async mt_unlocks_locks (mutex) fireRtmpCommandMessage_locked (
                conn, msg, method_name, amf_decoder);
        mutex.unlock ();
    }


  // ___________________________ Number of watchers ____________________________

  private:
    mt_mutex (mutex) Count num_watchers;

    static void informNumWatchersChanged (Events *events,
                                          void   *cb_data,
                                          void   *inform_data);

    static void stateInformNumWatchersChanged (StateEvents *state_events,
                                               void        *cb_data,
                                               void        *inform_data);

    mt_async mt_unlocks_locks (mutex) void fireNumWatchersChanged_locked ();

    static bool reportNumWatchersTask (void *_self);

    void notifyDeferred_NumWatchersChanged ();

    static void watcherDeletionCallback (void *_self);

  public:
    // 'guard_obj' must be unique. Only one pluOneWatcher() call should be made
    // for any 'guard_obj' instance. If 'guard_obj' is not null, then minusOneWatcher()
    // should not be called to cancel the effect of plusOneWatcher().
    // That will be done automatically when 'guard_obj' is destroyed.
    void plusOneWatcher (Object *guard_obj = NULL);
    mt_unlocks (mutex) void plusOneWatcher_locked (Object *guard_obj = NULL);

    void minusOneWatcher ();
    mt_unlocks (mutex) void minusOneWatcher_locked ();

    void plusWatchers (Count delta);
    mt_unlocks (mutex) void plusWatchers_locked (Count delta);

    void minusWatchers (Count delta);
    mt_unlocks (mutex) void minusWatchers_locked (Count delta);

    mt_mutex (mutex) bool getNumWatchers_locked () { return num_watchers; }


  // _____________________________ Stream binding ______________________________

  private:
    class BindLayer : public Referenced
    {
      public:
        bool valid;
        Uint64 priority;

        List< Ref<BindLayer> >::Element *list_el;

        WeakRef<VideoStream> weak_audio_stream;
        WeakRef<VideoStream> weak_video_stream;
        bool bind_audio;
        bool bind_video;
    };

    typedef List< Ref<BindLayer> > BindLayerList;

    BindLayerList bind_layer_list;
    BindLayerList::Element *cur_bind_el;

    class BindTicket : public Referenced
    {
      public:
        VideoStream    *video_stream;
        Ref<BindLayer>  bind_layer;
    };

    class BindInfo
    {
      public:
        Ref<BindTicket> cur_bind_ticket;
        bool propagate_close;

        Uint64 timestamp_offs;
        bool   got_timestamp_offs;
        WeakRef<VideoStream> weak_bind_stream;

        GenericInformer::SubscriptionKey bind_sbn;

        void reset ()
        {
            cur_bind_ticket    = NULL;
            propagate_close    = false;
            timestamp_offs     = 0;
            got_timestamp_offs = false;
            weak_bind_stream   = NULL;
            bind_sbn           = NULL;
        }

        BindInfo () { reset (); }
    };

    // TODO 1. got_stream_timestamp. 'stream_timestamp' may be unknown.
    //      2. set initial stream timestamp to some shifted value to compensate for possible slight timestamp drift,
    //         like +10 minutes.
    mt_mutex (mutex) Uint64 stream_timestamp_nanosec;

    mt_mutex (mutex) BindInfo abind;
    mt_mutex (mutex) BindInfo vbind;

    mt_mutex (mutex) bool pending_report_in_progress;
    mt_mutex (mutex) Count msg_inform_counter;
    mt_mutex (mutex) PendingEventList pending_event_list;

    mt_mutex (mutex) void bind_messageBegin (BindInfo     * mt_nonnull bind_info,
                                             MediaMessage * mt_nonnull msg);

    mt_iface (FrameSaver::FrameHandler)
      static FrameSaver::FrameHandler const bind_frame_handler;

      static mt_unlocks_locks (mutex) Result bind_savedAudioFrame (AudioMessage * mt_nonnull audio_msg,
                                                                   void         *_data);

      static mt_unlocks_locks (mutex) Result bind_savedVideoFrame (VideoMessage * mt_nonnull video_msg,
                                                                   void         *_data);

      static mt_unlocks_locks (mutex) Result bind_savedMediaDesc (MediaDesc_Audio *audio_desc,
                                                                  bool             set_audio,
                                                                  MediaDesc_Video *video_desc,
                                                                  bool             set_video,
                                                                  void            *_data);
    mt_iface_end

    mt_iface (VideoStream::Events)
      static Events const abind_events;
      static Events const vbind_events;

      static void bind_audioMessage (AudioMessage * mt_nonnull msg,
                                     void         *_bind_ticket);

      static void bind_videoMessage (VideoMessage * mt_nonnull msg,
                                     void         *_bind_ticket);

      static void bind_rtmpDataMessage (MediaMessage * mt_nonnull msg,
                                        void         *_bind_ticket);

      static void bind_playbackStart (void *_bind_ticket);

      static void bind_playbackStop (void *_bind_ticket);

      static void bind_closed (void *_bind_ticket);

      static void bind_mediaDesc (MediaDesc_Audio *audio_desc,
                                  bool             set_audio,
                                  MediaDesc_Video *video_desc,
                                  bool             set_video,
                                  void            *_bind_ticket);
    mt_iface_end

    void doBindPlaybackStop (BindTicket * mt_nonnull bind_ticket);

    mt_mutex (mutex) void doBindToStream (VideoStream *bind_audio_stream,
                                          VideoStream *bind_video_stream,
                                          bool         bind_audio,
                                          bool         bind_video);

    static bool closeTask (void *_self);

  public:
    void bindToStream (VideoStream *bind_audio_stream,
                       VideoStream *bind_video_stream,
                       bool         bind_audio,
                       bool         bind_video,
                       bool         propagate_close_audio = false,
                       bool         propagate_close_video = false);


  // _____________________________ Stream closing ______________________________

  private:
    mt_mutex (mutex) bool is_closed;

  public:
//#warning check usage of close() - it is mt_async
    mt_async void close_locked ();
    mt_async void close ();

    mt_mutex (mutex) bool isClosed_locked () { return is_closed; }


  // ____________________________ Publishing status ____________________________

  private:
    mt_mutex (mutex) bool publishing_in_progress;

    static void stateInformPublishingStatusChanged (StateEvents *state_events,
                                                    void        *cb_data,
                                                    void        *inform_data);

    mt_async mt_unlocks_locks (mutex) void firePublishingStatusChanged_locked (bool in_progress);

  public:
    mt_mutex (mutex) bool isPublishingInProgress_locked ()
        { return publishing_in_progress; }

    mt_const void init_setPublishingInProgress (bool const in_progress)
        { publishing_in_progress = in_progress; }

    void setPublishingInProgress (bool in_progress);


  // ____________________________ Stream parameters ____________________________

  private:
    mt_const Ref<StreamParameters> stream_params;

  public:
    bool hasParam (ConstMemory const name)
    {
        if (!stream_params)
            return false;

        return stream_params->hasParam (name);
    }

    ConstMemory getParam (ConstMemory   const name,
                          bool        * const ret_has_param = NULL)
    {
        if (!stream_params) {
            if (ret_has_param)
                *ret_has_param = false;

            return ConstMemory ();
        }

        return stream_params->getParam (name, ret_has_param);
    }

    mt_const void setStreamParameters (StreamParameters * const stream_params)
    {
        assert (!this->stream_params);
        this->stream_params = stream_params;
    }


  // _____________________________ Domain options ______________________________

  private:
    mt_const Ref<DomainOptions> domain_opts;

  public:
    mt_const void setDomainOptions (DomainOptions * const domain_opts) { this->domain_opts = domain_opts; }
    mt_const Ref<DomainOptions> getDomainOptions () { return domain_opts; }

  // ___________________________________________________________________________


  public:
    void lock   () { mutex.lock   (); }
    void unlock () { mutex.unlock (); }

    VideoStream (EmbedContainer    *embed_container,
                 DeferredProcessor * mt_nonnull deferred_processor,
                 bool               publishing_in_progress  = true,
                 bool               enable_frame_saver      = true,
//#warning TODO send_saved_video_frames from config parameter
                 bool               send_saved_video_frames = true);

    ~VideoStream ();
};

// TODO MediaStream should be the only name.
typedef VideoStream Stream;
typedef VideoStream MediaStream;

}


#endif /* LIBMOMENT__MEDIA_STREAM__H__ */

