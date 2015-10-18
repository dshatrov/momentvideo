/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>

#include <moment/libmoment.h>
#include <moment/moment_instance.h>

#include <moment-rtmp/rtmp_service.h>
#include <moment-rtmp/rtmpt_service.h>
#include <moment-rtmp/rtmp_push_protocol.h>
#include <moment-rtmp/rtmp_fetch_protocol.h>
#include <moment-rtmp/util_rtmp.h>


#define MOMENT_RTMP__FLOW_CONTROL


using namespace Moment;

namespace Moment {
namespace {

static LogGroup libMary_logGroup_mod_rtmp  ("mod_rtmp",           LogLevel::I);
static LogGroup libMary_logGroup_session   ("mod_rtmp.session",   LogLevel::I);
static LogGroup libMary_logGroup_framedrop ("mod_rtmp.framedrop", LogLevel::I);

namespace {
struct TranscodeEntry : public Referenced
{
    StRef<String> suffix;
    StRef<String> chain;
    Transcoder::TranscodingMode audio_mode;
    Transcoder::TranscodingMode video_mode;
};
}

namespace {
class StreamingParams
{
public:
    bool transcode;
    StRef<String> auth_key;

    void reset () { transcode = false; }
    StreamingParams () { reset (); }
};
}

namespace {
class WatchingParams
{
  public:
    bool start_paused;
    StRef<String> auth_key;

    Uint64 start_time_millisec;
    bool   got_start_time;

    Uint64 duration_millisec;
    bool   got_duration;

#if 0
DEPRECATED
    void reset ()
    {
	start_paused = default_start_paused;
        auth_key = NULL;

        start_time_millisec = 0;
        got_start_time = false;

        duration_millisec = 0;
        got_duration = false;
    }
#endif

    WatchingParams (bool const start_paused)
        : start_paused (start_paused)
    {}

  private:
    WatchingParams (); // DEPRECATED { reset (); }
};
}

StatCounter_AtomicInt stat_counter__rtmp_client_session (0, "rtmp_client_session", 0, "rtmp");

namespace {
  class ClientSession;
}

typedef List< Ref<ClientSession> > StreamList;

namespace {
class StreamHashEntry : public HashEntry<>
{
public:
    mt_const StRef<String> stream_name;
    mt_mutex (mutex) StreamList session_list;
};
}

typedef Hash< StreamHashEntry,
              Memory,
              MemberExtractor< StreamHashEntry,
                               StRef<String>,
                               &StreamHashEntry::stream_name,
                               Memory,
                               AccessorExtractor< String,
                                                  Memory,
                                                  &String::mem > >,
              MemoryComparator<> >
        StreamHash;

namespace {
class MomentRtmpModule;
}

namespace {
class ClientSession : public Object
{
  public:
    StateMutex mutex;

    StatCounter_AtomicInt::Unit<&stat_counter__rtmp_client_session> stat_unit;

#warning TODO get rid of this WeakRef, use obj in CbDesc instead
    mt_const WeakRef<MomentRtmpModule> weak_rtmp_module;

    mt_const Ref<PagePool> page_pool;
    mt_const Uint64 paused_avc_interframes;

    mt_mutex (mutex) bool valid;

    AtomicInt playing;

    mt_const IpAddress client_addr;

    mt_const Ref<RtmpConnection> rtmp_conn;

    FlvMuxer   flv_muxer;
    Mp4AvMuxer mp4_muxer;
    Embed<AvRecorder> recorder;

    mt_mutex (mutex) StRef<String> referer_page_url;
    mt_mutex (mutex) StRef<String> app_name;

    mt_mutex (mutex) StRef<String> stream_name;

    mt_mutex (mutex) StreamHashEntry *streaming_hash_entry;
    mt_mutex (mutex) StreamList::Element *streaming_list_el;

    mt_mutex (mutex) Ref<VideoStream> video_stream;
    mt_mutex (mutex) Ref<Transcoder> transcoder;
    mt_mutex (mutex) List<StreamManager::StreamKey> out_stream_keys;

    mt_mutex (mutex) Ref<MediaSource> watching_media_source;
    mt_mutex (mutex) bool vod_mode;
    mt_mutex (mutex) bool vod_source_started; // TODO Unused?

    mt_mutex (mutex) bool vod_sender_blocked;
    mt_mutex (mutex) bool vod_ratelimit_blocked;

    mt_sync_domain (VodSource) Uint64 last_vod_audio_timestamp_nanosec;
    mt_sync_domain (VodSource) Uint64 last_vod_video_timestamp_nanosec;
    mt_sync_domain (VodSource) RateLimit vod_ratelimit;

    mt_mutex (mutex) StreamingParams streaming_params;
    mt_mutex (mutex) WatchingParams watching_params;

    mt_mutex (mutex) bool is_publishing_in_progress;

    #ifdef MOMENT_RTMP__FLOW_CONTROL
      mt_mutex (mutex) bool overloaded;
    #endif

    mt_mutex (mutex) Count no_keyframe_counter;
    mt_mutex (mutex) bool keyframe_sent;
    mt_mutex (mutex) bool first_keyframe_sent;
    mt_mutex (mutex) Uint64 first_interframes_sent;

    mt_mutex (mutex) bool resumed;

    mt_mutex (mutex) PagePool *paused_keyframe_page_pool;
    mt_mutex (mutex) PagePool::Page *paused_keyframe_page;

    // Synchronized by rtmp_server.
    bool streaming;
    bool watching;

    void doResume ();

    ClientSession (EmbedContainer * const embed_container,
                   bool             const start_paused,
                   PagePool       * const mt_nonnull page_pool,
                   Uint64           const paused_avc_interframes)
        : Object                    (embed_container),
          page_pool                 (page_pool),
          paused_avc_interframes    (false),
	  valid                     (true),
          playing                   (0),
	  recorder                  (this /* embed_container */),
          streaming_hash_entry      (NULL),
          streaming_list_el         (NULL),
          vod_mode                  (false),
          vod_source_started        (false),
          vod_sender_blocked        (false),
          vod_ratelimit_blocked     (false),
          last_vod_audio_timestamp_nanosec (0),
          last_vod_video_timestamp_nanosec (0),
          watching_params           (start_paused),
          is_publishing_in_progress (false),
        #ifdef MOMENT_RTMP__FLOW_CONTROL
	  overloaded                (false),
        #endif
	  no_keyframe_counter       (0),
	  keyframe_sent             (false),
	  first_keyframe_sent       (false),
          first_interframes_sent    (0),
	  resumed                   (false),
          paused_keyframe_page_pool (NULL),
          paused_keyframe_page      (NULL),
	  streaming                 (false),
	  watching                  (false)
    {
	logD_ (_func, "session ", _ptr (this));
    }

    ~ClientSession ()
    {
	logD_ (_func, "session ", _ptr (this));

        mutex.lock ();
        if (vod_mode && watching_media_source) {
            VodSource * const vod_source = static_cast <VodSource*> (watching_media_source.ptr());
            vod_source->stop ();
        }

        if (paused_keyframe_page) {
            assert (paused_keyframe_page_pool);
            paused_keyframe_page_pool->pageUnref (paused_keyframe_page);
            paused_keyframe_page = NULL;
            paused_keyframe_page_pool = NULL;
        }
        mutex.unlock ();
    }
};
}

namespace {
class MomentRtmpModule : public Object
{
#warning TODO private
  public:
    StateMutex mutex;

    mt_const Ref<StreamManager> stream_manager;
    mt_const Ref<ClientManager> client_manager;
    mt_const Ref<MomentServer>  moment_server;

    Embed<RtmpService>  rtmp_service;
    Embed<RtmptService> rtmpt_service;

    mt_const bool trim_app_name      = true;
    mt_const bool rtmp_sample_access = true;
    mt_const bool dump_amf           = false;
    mt_const bool record_all         = false;
    mt_const bool record_live        = false;

    mt_const bool audio_waits_video    = false;
    mt_const bool wait_for_keyframe    = true;
    mt_const bool default_start_paused = false;
    mt_const bool record_mp4           = false;
    mt_const Uint64 paused_avc_interframes = 3;

    mt_const RateLimitParams ratelimit_params;

    mt_const String record_path =
          #ifdef LIBMARY_PLATFORM_MACOSX
            "/Applications/MomentVideoServer.app/records/"
          #else
            "/opt/moment/records/"
          #endif
            ;
    mt_const Uint64 recording_limit = 1 << 24 /* 16 Mb */;
    mt_const Uint64 record_permissions = LIBMARY__DEFAULT_CREATE_PERMISSIONS;

    mt_const Count no_keyframe_limit = 250; // 25 fps * 10 seconds

    mt_const Ref<MomentServer> moment;
    mt_const Ref<Timers>       timers;
    mt_const Ref<PagePool>     page_pool;

    mt_const Stat::ParamKey stat_num_sessions;

    // TODO List<TranscodeEntry>
    typedef List< Ref<TranscodeEntry> > TranscodeList;

    mt_const TranscodeList transcode_list;
    mt_const bool   transcode_on_demand = true;
    mt_const Uint64 transcode_on_demand_timeout_millisec = 5000;

    mt_mutex (mutex) StreamHash streaming_hash;

    void destroyClientSession (ClientSession *client_session);

    mt_mutex (client_session->mutex) void startTranscoder (ClientSession *client_session);

    mt_iface (MediaStream::Events)
      static VideoStream::Events const out_stream_events;

      static void out_audioMessage (AudioMessage * mt_nonnull msg,
                                    void         *_session);

      static void out_videoMessage (VideoMessage * mt_nonnull msg,
                                    void         *_session);

      static void out_rtmpDataMessage (MediaMessage * mt_nonnull msg,
                                       void         *_session);
    mt_iface_end

    mt_iface (MediaStream::StateEvents)
      static MediaStream::StateEvents const out_stream_state_events;

      static void out_streamClosed (void *_session);

      static void out_streamPublishingStatusChanged (bool  publishing_in_progress,
                                                     void *_session);
    mt_iface_end

    Result sendStreamStartMessages (RtmpConnection * mt_nonnull rtmp_conn,
                                    Uint32          msg_stream_id,
                                    ConstMemory     stream_name,
                                    bool            is_vod);

    Result completeStartStreaming (ClientSession *client_session,
                                   ConstMemory    stream_name,
                                   RecordingMode  rec_mode,
                                   VideoStream   *stream,
                                   Uint32         msg_stream_id,
                                   double         transaction_id);

    Result completeStartWatching (MediaSource   * mt_nonnull media_source,
                                  ClientSession * mt_nonnull client_session,
                                  ConstMemory    stream_name,
                                  Uint32         msg_stream_id);

    Result completeStartWatching_MediaStream (MediaStream   * mt_nonnull stream,
                                              ClientSession * mt_nonnull client_session,
                                              ConstMemory    stream_name,
                                              Uint32         msg_stream_id);

    Result completeStartWatching_VodSource (VodSource     * mt_nonnull vod_source,
                                            ClientSession * mt_nonnull client_session,
                                            ConstMemory    stream_name,
                                            Uint32         msg_stream_id);

    Result doPlay (ClientSession * mt_nonnull client_session,
                   Uint32         msg_stream_id,
                   AmfDecoder    * mt_nonnull decoder);

    Result doPublish (ClientSession * mt_nonnull client_session,
                      Uint32         msg_stream_id,
                      AmfDecoder    * mt_nonnull decoder,
                      RtmpConnection::ConnectionInfo * mt_nonnull conn_info);

  // __________________________________ Admin __________________________________

    static HttpService::HttpHandler const admin_http_handler;

    static Result adminHttpRequest (HttpRequest  * mt_nonnull req,
                                    HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                                    IpAddress     local_addr,
                                    Sender       * mt_nonnull conn_sender,
                                    Memory        msg_body,
                                    void        ** mt_nonnull ret_msg_data,
                                    void         *_self);

  // ___________________________________________________________________________


    void resetWatchingParams (WatchingParams * const mt_nonnull watching_params)
    {
        watching_params->start_paused = default_start_paused;
        watching_params->auth_key = NULL;

        watching_params->start_time_millisec = 0;
        watching_params->got_start_time = false;

        watching_params->duration_millisec = 0;
        watching_params->got_duration = false;
    }

    mt_const Result init (MomentServer * const mt_nonnull moment_server)
    {
        timers         = moment_server->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();
        page_pool      = moment_server->getPagePool();
        stream_manager = moment_server->getStreamManager();
        client_manager = moment_server->getClientManager();
        this->moment_server = moment_server;

        stat_num_sessions = getStat()->createParam ("mod_rtmp/num_sessions",
                                                    "Number of active RTMP(T) sessions",
                                                    Stat::ParamType_Int64,
                                                    0,
                                                    0.0);

        Ref<MConfig::Config> const config = moment_server->getConfigManager()->getConfig();

        if (   !configGetBoolean_ (config, "mod_rtmp/trim_app_name",      &trim_app_name,      _func)
            || !configGetBoolean_ (config, "mod_rtmp/rtmp_sample_access", &rtmp_sample_access, _func)
            || !configGetBoolean_ (config, "mod_rtmp/dump_amf",           &dump_amf,           _func)
            || !configGetBoolean_ (config, "mod_rtmp/record_all",         &record_all,         _func)
            || !configGetBoolean_ (config, "mod_rtmp/record_live",        &record_live,        _func))
        {
            return Result::Failure;
        }

        record_path.set (config->getString_default ("mod_rtmp/record_path", record_path.mem()));
        {
          // Adding a slash at the end, if missing.
          #ifdef LIBMARY_PLATFORM_WIN32
            char slash_char = '\\';
          #else
            char slash_char = '/';
          #endif
            if (record_path.len() == 0
                || record_path.buf() [record_path.len() - 1] != (Byte) slash_char)
            {
                record_path = makeStringVal (record_path, ConstMemory::forObject (slash_char));
            }
        }

        if (   !configGetUint64_  (config, "mod_rtmp/record_max_size",        &recording_limit,        _func)
            || !configGetUint64_  (config, "mod_rtmp/record_permissions",     &record_permissions,     _func)
            || !configGetBoolean_ (config, "mod_rtmp/audio_waits_video",      &audio_waits_video,      _func)
            || !configGetBoolean_ (config, "mod_rtmp/wait_for_keyframe",      &wait_for_keyframe,      _func)
            || !configGetBoolean_ (config, "mod_rtmp/start_paused",           &default_start_paused,   _func)
            || !configGetBoolean_ (config, "mod_rtmp/record_mp4",             &record_mp4,             _func)
            || !configGetUint64_  (config, "mod_rtmp/paused_avc_interframes", &paused_avc_interframes, _func))
        {
            return Result::Failure;
        }

        if (MConfig::Section * const ratelimit_section = config->getSection ("ratelimit")) {
            if (!parseRatelimitConfig (ratelimit_section, &ratelimit_params))
                return Result::Failure;
        }
        if (MConfig::Section * const ratelimit_section = config->getSection ("mod_rtmp/ratelimit")) {
            if (!parseRatelimitConfig (ratelimit_section, &ratelimit_params))
                return Result::Failure;
        }

        if (MConfig::Section * const modrtmp_section = config->getSection ("mod_rtmp")) {
            MConfig::Section::iter iter (*modrtmp_section);
            while (!modrtmp_section->iter_done (iter)) {
                MConfig::SectionEntry * const sect_entry = modrtmp_section->iter_next (iter);
                if (sect_entry->getType() == MConfig::SectionEntry::Type_Section
                    && equal (sect_entry->getName(), "transcode"))
                {
                    MConfig::Section * const transcode_section = static_cast <MConfig::Section*> (sect_entry);

                    ConstMemory suffix;
                    if (MConfig::Option * const opt = transcode_section->getOption ("suffix"))
                        if (MConfig::Value * const val = opt->getValue())
                            suffix = val->mem();

                    ConstMemory chain;
                    if (MConfig::Option * const opt = transcode_section->getOption ("chain"))
                        if (MConfig::Value * const val = opt->getValue())
                            chain = val->mem();

                    Transcoder::TranscodingMode audio_mode = Transcoder::TranscodingMode_On;
                    Transcoder::TranscodingMode video_mode = Transcoder::TranscodingMode_On;

                    if (MConfig::Option * const opt = transcode_section->getOption ("direct_audio")) {
                        MConfig::BooleanValue const opt_val = opt->getBoolean();
                        if (opt_val == MConfig::Boolean_Invalid)
                            logE_ (_func, "Invalid value for config option direct_audio");
                        else
                        if (opt_val == MConfig::Boolean_True)
                            audio_mode = Transcoder::TranscodingMode_Direct;
                    }

                    if (MConfig::Option * const opt = transcode_section->getOption ("direct_video")) {
                        MConfig::BooleanValue const opt_val = opt->getBoolean();
                        if (opt_val == MConfig::Boolean_Invalid)
                            logE_ (_func, "Invalid value for config option direct_video");
                        else
                        if (opt_val == MConfig::Boolean_True)
                            video_mode = Transcoder::TranscodingMode_Direct;
                    }

                    if (MConfig::Option * const opt = transcode_section->getOption ("no_audio")) {
                        MConfig::BooleanValue const opt_val = opt->getBoolean();
                        if (opt_val == MConfig::Boolean_Invalid)
                            logE_ (_func, "Invalid value for config option no_audio");
                        else
                        if (opt_val == MConfig::Boolean_True)
                            audio_mode = Transcoder::TranscodingMode_Off;
                    }

                    if (MConfig::Option * const opt = transcode_section->getOption ("no_video")) {
                        MConfig::BooleanValue const opt_val = opt->getBoolean();
                        if (opt_val == MConfig::Boolean_Invalid)
                            logE_ (_func, "Invalid value for config option no_video");
                        else
                        if (opt_val == MConfig::Boolean_True)
                            video_mode = Transcoder::TranscodingMode_Off;
                    }

                    Ref<TranscodeEntry> const transcode_entry = grab (new (std::nothrow) TranscodeEntry);
                    transcode_entry->suffix = st_grab (new (std::nothrow) String (suffix));
                    transcode_entry->chain  = st_grab (new (std::nothrow) String (chain));
                    transcode_entry->audio_mode = audio_mode;
                    transcode_entry->video_mode = video_mode;

                    transcode_list.append (transcode_entry);
                }
            }
        }

        {
            ConstMemory const opt_name = "mod_rtmp/transcode_on_demand";
            MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
            if (opt_val == MConfig::Boolean_Invalid)
                logE_ (_func, "Invalid value for config option ", opt_name);
            else
            if (opt_val == MConfig::Boolean_False)
                transcode_on_demand = false;
            else
                transcode_on_demand = true;

            logI_ (_func, opt_name, ": ", transcode_on_demand);
        }

        {
            ConstMemory const opt_name = "mod_rtmp/transcode_on_demand_timeout";
            MConfig::GetResult const res = config->getUint64_default (
                    opt_name, &transcode_on_demand_timeout_millisec, transcode_on_demand_timeout_millisec);
            if (!res)
                logE_ (_func, "bad value for ", opt_name);

            logI_ (_func, opt_name, ": ", transcode_on_demand_timeout_millisec);
        }

        return Result::Success;
    }

    MomentRtmpModule (EmbedContainer * const embed_container)
        : Object        (embed_container),
          rtmp_service  (this /* embed_container */),
          rtmpt_service (this /* embed_container */)
    {}

    ~MomentRtmpModule ();
};
}

MomentRtmpModule::~MomentRtmpModule ()
{
    {
        StreamHash::iterator iter (streaming_hash);
        while (!iter.done()) {
            StreamHashEntry * const entry = iter.next ();
            streaming_hash.remove (entry);
            delete entry;
        }
    }
}

Result
MomentRtmpModule::sendStreamStartMessages (RtmpConnection * const mt_nonnull rtmp_conn,
                                           Uint32           const msg_stream_id,
                                           ConstMemory      const stream_name,
                                           bool             const is_vod)
{
    if (!RtmpUtil::sendNetStreamPlayReset (rtmp_conn, msg_stream_id, stream_name)) {
        logE_ (_func, "sendNetStreamPlayReset() failed");
        return Result::Failure;
    }

    if (is_vod)
        rtmp_conn->sendUserControl_StreamIsRecorded (msg_stream_id);

    rtmp_conn->sendUserControl_StreamBegin (msg_stream_id);

    if (!RtmpUtil::sendNetStreamPlayStart (rtmp_conn, msg_stream_id, stream_name)) {
        logE_ (_func, "sendNetStreamPlayStart() failed");
        return Result::Failure;
    }

    if (rtmp_sample_access)
        RtmpUtil::sendRtmpSampleAccess (rtmp_conn, msg_stream_id, true, true);

    if (!RtmpUtil::sendNetStreamDataStart (rtmp_conn, msg_stream_id)) {
        logE_ (_func, "sendNetStreamDataStart() failed");
        return Result::Failure;
    }

    // TEST
    // rtmp_conn->sendUserControl_BufferReady (msg_stream_id);

    return Result::Success;
}

static mt_mutex (client_session) Result savedAudioFrame (AudioMessage * mt_nonnull audio_msg,
                                                         void         *_client_session);

static mt_mutex (client_session) Result savedVideoFrame (VideoMessage * mt_nonnull video_msg,
                                                         void         *_client_session);

static FrameSaver::FrameHandler const saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame,
    NULL /* mediaDesc */
};

void
ClientSession::doResume ()
{
    mutex.lock ();
    if (!resumed) {
        resumed = true;
        if (video_stream) {
            video_stream->lock ();
            video_stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, this);
            video_stream->unlock ();

            if (paused_keyframe_page) {
                assert (paused_keyframe_page_pool);
                paused_keyframe_page_pool->pageUnref (paused_keyframe_page);
                paused_keyframe_page_pool = NULL;
                paused_keyframe_page = NULL;
            }
        }
    }
    mutex.unlock ();
}

void
MomentRtmpModule::destroyClientSession (ClientSession * const client_session)
{
    logD_ (_func, "client_session 0x", fmt_hex, (UintPtr) client_session);

    mutex.lock ();
    client_session->mutex.lock ();
    if (!client_session->valid) {
        mutex.unlock ();
	client_session->mutex.unlock ();
	logD_ (_func, "invalid session");
	return;
    }
    client_session->valid = false;

    if (client_session->streaming_hash_entry) {
        assert (client_session->streaming_list_el);

        client_session->streaming_hash_entry->session_list.remove (client_session->streaming_list_el);
        if (client_session->streaming_hash_entry->session_list.isEmpty())
            streaming_hash.remove (client_session->streaming_hash_entry);

        client_session->streaming_hash_entry = NULL;
        client_session->streaming_list_el = NULL;
    } else {
        assert (!client_session->streaming_list_el);
    }
    mutex.unlock ();

    client_session->mutex.unlock ();

    client_session->recorder->stop();

    client_session->mutex.lock ();
    getStat()->addInt (stat_num_sessions, -1);

    {
        List<StreamManager::StreamKey>::iter iter (client_session->out_stream_keys);
        while (!client_session->out_stream_keys.iter_done (iter)) {
            StreamManager::StreamKey &stream_key = client_session->out_stream_keys.iter_next (iter)->data;
            stream_manager->removeStream (stream_key);
        }
        client_session->out_stream_keys.clear ();
    }

    Ref<VideoStream> const video_stream = client_session->video_stream;

    client_session->mutex.unlock ();

    // Closing video stream *after* firing clientDisconnected() to avoid
    // premature closing of client connections in streamClosed().
    if (video_stream)
	video_stream->close ();

    client_session->unref ();
}

mt_mutex (client_session->mutex) void
MomentRtmpModule::startTranscoder (ClientSession * const client_session)
{
    if (!client_session->streaming_params.transcode ||
        !client_session->video_stream ||
        client_session->transcoder)
    {
        logD_ (_func, "no transcoding for stream \"", client_session->stream_name, "\"");
        return;
    }

    Ref<Transcoder> transcoder;
    if (!transcode_list.isEmpty()) {
        transcoder = moment->getTranscodingManager()->newTranscoder();
        if (!transcoder) {
            logD_ (_func, "transcoder module is missing");
            return;
        }

        transcoder->init (moment->getServerApp()->getServerContext()->
                                  getMainThreadContext()->getDeferredProcessor(),
                          timers,
                          moment->getPagePool(),
                          transcode_on_demand,
                          transcode_on_demand_timeout_millisec);

        TranscodeList::iter iter (transcode_list);
        while (!transcode_list.iter_done (iter)) {
            Ref<TranscodeEntry> &transcode_entry = transcode_list.iter_next (iter)->data;

//#warning TODO Transcoded VideoStream remains available (hanging) after source stream has been closed.

            StRef<String> const out_stream_name =
                    makeString (client_session->stream_name ?
                                        client_session->stream_name->mem() : ConstMemory(),
                                transcode_entry->suffix->mem());
            Ref<VideoStream> const out_stream =
                    grabNewObject <MediaStream> (
                            moment_server->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());
            {
                Ref<StreamParameters> const stream_params = grab (new (std::nothrow) StreamParameters);
                if (transcode_entry->audio_mode == Transcoder::TranscodingMode_Off)
                    stream_params->setParam ("no_audio", "true");
                if (transcode_entry->video_mode == Transcoder::TranscodingMode_Off)
                    stream_params->setParam ("no_video", "true");

                out_stream->setStreamParameters (stream_params);
            }

            transcoder->addOutputStream (out_stream,
                                         transcode_entry->chain->mem(),
                                         transcode_entry->audio_mode,
                                         transcode_entry->video_mode);

            StreamManager::StreamKey const out_stream_key =
                    moment->getStreamManager()->addStream (out_stream, out_stream_name->mem());
            client_session->out_stream_keys.append (out_stream_key);
        }

        transcoder->start (client_session->video_stream);
    }

    client_session->transcoder = transcoder;
}

VideoStream::Events const MomentRtmpModule::out_stream_events = {
    out_audioMessage,
    out_videoMessage,
    out_rtmpDataMessage,
    NULL /* rtmpCommandMessage */,
    NULL /* streamClosed */,
    NULL /* numWatchersChanged */,
    NULL /* media_desc */
};

void
MomentRtmpModule::out_audioMessage (AudioMessage * const mt_nonnull msg,
                                    void         * const _session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_session);
    Ref<MomentRtmpModule> const self = client_session->weak_rtmp_module.getRef ();
    if (!self)
        return;

    client_session->mutex.lock ();

    if (msg->frame_type == AudioFrameType::RawData) {
        if (!client_session->resumed) {
            client_session->mutex.unlock ();
            return;
        }

#warning TODO Move config vars like this into ClientSession and abolish weak_rtmp_module.
#warning TODO ClientSession should likely be a separate entity.
        if (self->audio_waits_video) {
            if (!client_session->first_keyframe_sent) {
                client_session->mutex.unlock ();
                return;
            }
        }
    }

  #ifdef MOMENT_RTMP__FLOW_CONTROL
    if (client_session->overloaded
	&& msg->frame_type == AudioFrameType::RawData)
    {
      // Connection overloaded, dropping this audio frame.
	logD (framedrop, _func, "Connection overloaded, dropping audio frame");
	client_session->mutex.unlock ();
	return;
    }
  #endif

    client_session->mutex.unlock ();

    client_session->rtmp_conn->sendAudioMessage (msg);
}

void
MomentRtmpModule::out_videoMessage (VideoMessage * const mt_nonnull msg,
                                    void         * const _session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_session);
    Ref<MomentRtmpModule> const self = client_session->weak_rtmp_module.getRef ();
    if (!self)
        return;

/*
    logS_ (_func, "ts ", msg->timestamp_nanosec, " ", msg->frame_type);
    logS_ (_func, "fks: ", client_session->first_keyframe_sent, ", "
           "ks: ", client_session->keyframe_sent, ", "
           "nkfk: ", client_session->no_keyframe_counter, ", "
           "moff: ", msg->msg_offset);
*/

    client_session->mutex.lock ();

  #ifdef MOMENT_RTMP__FLOW_CONTROL
    if (client_session->overloaded
	&& (   msg->frame_type == VideoFrameType::KeyFrame
	    || msg->frame_type == VideoFrameType::InterFrame
	    || msg->frame_type == VideoFrameType::DisposableInterFrame
	    || msg->frame_type == VideoFrameType::GeneratedKeyFrame))
    {
      // Connection overloaded, dropping this video frame. In general, we'll
      // have to wait for the next keyframe after we've dropped a frame.
      // We do not care about disposable frames yet.

	logD (framedrop, _func, "Connection overloaded, dropping video frame");

	client_session->no_keyframe_counter = 0;
	client_session->keyframe_sent = false;

	client_session->mutex.unlock ();
	return;
    }
  #endif // MOMENT_RTMP__FLOW_CONTROL

    bool got_keyframe = false;
    if (msg->frame_type == VideoFrameType::KeyFrame ||
        msg->frame_type == VideoFrameType::GeneratedKeyFrame)
    {
	got_keyframe = true;
    } else
    if (msg->frame_type == VideoFrameType::AvcSequenceHeader ||
        msg->frame_type == VideoFrameType::AvcEndOfSequence)
    {
        client_session->keyframe_sent = false;
    } else
    if (!client_session->keyframe_sent
	&& (   msg->frame_type == VideoFrameType::InterFrame
	    || msg->frame_type == VideoFrameType::DisposableInterFrame))
    {
	++client_session->no_keyframe_counter;
	if (client_session->no_keyframe_counter >= self->no_keyframe_limit) {
            logD_ (_func, "no_keyframe_limit hit: ", client_session->no_keyframe_counter);
	    got_keyframe = true;
	} else
        if (self->wait_for_keyframe) {
	  // Waiting for a keyframe, dropping current video frame.
            logS_ (_func, "wait_for_keyframe, dropping");
	    client_session->mutex.unlock ();
	    return;
	}
    }

    if (got_keyframe)
	client_session->no_keyframe_counter = 0;

    if (!client_session->resumed &&
        msg->frame_type.isVideoData())
    {
        if (msg->frame_type.isInterFrame())
        {
            if (msg->codec_id != VideoCodecId::AVC
                || client_session->first_interframes_sent >= self->paused_avc_interframes
                || !client_session->first_keyframe_sent)
            {
                client_session->mutex.unlock ();
                return;
            }
            ++client_session->first_interframes_sent;
        } else {
            assert (msg->frame_type.isKeyFrame());

            if (client_session->first_keyframe_sent) {
                client_session->keyframe_sent = false;
                client_session->mutex.unlock ();
                return;
            }

            if (client_session->paused_keyframe_page) {
                assert (client_session->paused_keyframe_page_pool);
                client_session->paused_keyframe_page_pool->pageUnref (client_session->paused_keyframe_page);
                client_session->paused_keyframe_page = NULL;
                client_session->paused_keyframe_page_pool = NULL;
            }

            client_session->paused_keyframe_page_pool = msg->page_pool;
            client_session->paused_keyframe_page = msg->page_list.first;
            msg->page_pool->pageRef (msg->page_list.first);
        }
    }

    if (got_keyframe) {
	client_session->first_keyframe_sent = true;
	client_session->keyframe_sent = true;
    }

    client_session->mutex.unlock ();

    client_session->rtmp_conn->sendVideoMessage (msg);
}

void
MomentRtmpModule::out_rtmpDataMessage (MediaMessage * const mt_nonnull msg,
                                       void         * const _session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_session);

    logD_ (_func, "session 0x", fmt_hex, (UintPtr) client_session);

    client_session->rtmp_conn->sendDataMessage_AMF0_Pages (RtmpConnection::DefaultMessageStreamId,
                                                           &msg->page_list,
                                                           msg->msg_offset,
                                                           msg->msg_len);
}

MediaStream::StateEvents const MomentRtmpModule::out_stream_state_events = {
    out_streamClosed,
    NULL /* numWatchersChanged */,
    out_streamPublishingStatusChanged,
    NULL /* mediaDesc */
};

void
MomentRtmpModule::out_streamClosed (void * const _session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_session);
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) client_session);
    client_session->rtmp_conn->closeAfterFlush ();
}

void
MomentRtmpModule::out_streamPublishingStatusChanged (bool   const publishing_in_progress,
                                                     void * const _session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_session);
    Ref<MomentRtmpModule> const self = client_session->weak_rtmp_module.getRef ();
    if (!self)
        return;

    logD_ (_func, "session 0x", fmt_hex, (UintPtr) client_session, ", "
           "publishing_in_progress: ", publishing_in_progress);

    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        return;
    }

    if (client_session->is_publishing_in_progress != publishing_in_progress) {
        client_session->is_publishing_in_progress = publishing_in_progress;

        logD_ (_func, "sending publishing notificationm, publishing_in_progres: ", publishing_in_progress);
        if (publishing_in_progress) {
            if (!RtmpUtil::sendNetStreamPlayPublishNotify (client_session->rtmp_conn,
                                                           RtmpConnection::DefaultMessageStreamId,
                                                           String::mem (client_session->stream_name)))
            {
                client_session->mutex.unlock ();
                client_session->rtmp_conn->reportError ();
                return;
            }

            if (!self->sendStreamStartMessages (client_session->rtmp_conn,
                                                RtmpConnection::DefaultMessageStreamId,
                                                String::mem (client_session->stream_name),
                                                false /* is_vod */))
            {
                client_session->mutex.unlock ();
                client_session->rtmp_conn->reportError ();
                return;
            }
        } else {
            if (!RtmpUtil::sendNetStreamPlayUnpublishNotify (client_session->rtmp_conn,
                                                             RtmpConnection::DefaultMessageStreamId,
                                                             String::mem (client_session->stream_name)))
            {
                client_session->mutex.unlock ();
                client_session->rtmp_conn->reportError ();
                return;
            }
        }
    }

    client_session->mutex.unlock ();
}

static void startRtmpStreaming_paramCallback (ConstMemory   const name,
                                              ConstMemory   const value,
                                              void        * const _streaming_params)
{
    StreamingParams * const streaming_params = static_cast <StreamingParams*> (_streaming_params);

    logD_ (_func, "name: ", name, ", value: ", value);

    if (equal (name, "transcode"))
        streaming_params->transcode = true;
    if (equal (name, "auth"))
        streaming_params->auth_key = newString (value);
}

static void startRtmpWatching_paramCallback (ConstMemory   const name,
                                             ConstMemory   const value,
                                             void        * const _watching_params)
{
    WatchingParams * const watching_params = static_cast <WatchingParams*> (_watching_params);

    logD_ (_func, name, " = ", value);

    if (equal (name, "paused")) {
        watching_params->start_paused = true;
    } else
    if (equal (name, "auth")) {
        watching_params->auth_key = newString (value);
    } else
    if (equal (name, "t")) {
        if (!strToUint64_safe (value, &watching_params->start_time_millisec, 10)) {
            watching_params->start_time_millisec = 0;
            watching_params->got_start_time = false;
        } else {
            watching_params->got_start_time = true;
        }
    } else
    if (equal (name, "d")) {
        if (!strToUint64_safe (value, &watching_params->duration_millisec, 10)) {
            watching_params->duration_millisec = 0;
            watching_params->got_duration = false;
        } else {
            watching_params->got_duration = true;
        }
    }
}

namespace {
struct StartStreamingCallback_Data : public Referenced
{
    MomentRtmpModule       *moment_rtmp_module;
    WeakRef<ClientSession>  weak_client_session;
    StRef<String>           stream_name;
    RecordingMode           rec_mode;
    Ref<VideoStream>        video_stream;
    Uint32                  msg_stream_id;
    double                  transaction_id;
};
}

static void startStreamingCallback (Result                         const res,
                                    ClientManager::ClientSession * const /* cm_session */,
                                    void                         * const _data)
{
    StartStreamingCallback_Data * const data = static_cast <StartStreamingCallback_Data*> (_data);
    MomentRtmpModule * const self = data->moment_rtmp_module;

    Ref<ClientSession> const client_session = data->weak_client_session.getRef ();
    if (!client_session) {
        logD (mod_rtmp, _func, "client session gone");
        return;
    }

    if (!res) {
        logD (mod_rtmp, _func, "streaming not allowed");
        client_session->rtmp_conn->reportError ();
        return;
    }

    if (!self->completeStartStreaming (client_session,
                                       data->stream_name->mem(),
                                       data->rec_mode,
                                       data->video_stream,
                                       data->msg_stream_id,
                                       data->transaction_id))
    {
        client_session->rtmp_conn->reportError ();
    }
}

Result
MomentRtmpModule::completeStartStreaming (ClientSession * const client_session,
                                          ConstMemory     const stream_name,
                                          RecordingMode   const rec_mode,
                                          VideoStream   * const stream,
                                          Uint32          const msg_stream_id,
                                          double          const /* transaction_id */)
{
    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        return Result::Success;
    }
    client_session->mutex.unlock ();

    if (record_all) {
	logD_ (_func, "rec_mode: ", rec_mode);
	if (record_live
            || rec_mode == RecordingMode::Replace
	    || rec_mode == RecordingMode::Append)
	{
            StRef<String> const filename =
                    makeString (record_path,
                                stream_name,
                                (record_mp4 ? ConstMemory (".mp4") : ConstMemory (".flv")));

	    logD_ (_func, "recording, stream: 0x", fmt_hex, (UintPtr) stream, ", filename: ", filename);

	    // TODO Support "append" mode.
	    client_session->recorder->setVideoStream (stream);
	    client_session->recorder->start (filename->mem(), record_permissions);
	}
    }

    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        client_session->recorder->stop();
        return Result::Success;
    }

    startTranscoder (client_session);
    client_session->mutex.unlock ();

    // TODO Subscribe for translation stop? (probably not here)

    client_session->rtmp_conn->sendUserControl_StreamBegin (msg_stream_id);

    if (!RtmpUtil::sendNetStreamPublishStart (client_session->rtmp_conn,
                                              msg_stream_id,
                                              stream_name))
    {
        logE_ (_func, "sendNetStreamPublishStart() failed");
        return Result::Failure;
    }

    return Result::Success;
}

static mt_mutex (client_session) Result
savedAudioFrame (AudioMessage * const mt_nonnull audio_msg,
                 void         * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (!client_session->resumed
	&& audio_msg->frame_type == AudioFrameType::RawData)
    {
        return Result::Success;
    }

    client_session->rtmp_conn->sendAudioMessage (audio_msg);

    return Result::Success;
}

static mt_mutex (client_session) Result
savedVideoFrame (VideoMessage * const mt_nonnull video_msg,
                 void         * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

/*
    logS_ (_func, "ts ", video_msg->timestamp_nanosec, " ", video_msg->frame_type, ", "
           "fks: ",  client_session->first_keyframe_sent, ", "
           "ks: ",   client_session->keyframe_sent, ", "
           "nkfk: ", client_session->no_keyframe_counter);
*/

    if (!client_session->resumed
        && video_msg->frame_type.isInterFrame())
    {
        if (video_msg->codec_id != VideoCodecId::AVC
            || client_session->first_interframes_sent >= client_session->paused_avc_interframes
            || !client_session->first_keyframe_sent)
        {
            return Result::Success;
        }
        ++client_session->first_interframes_sent;
    }

    if (video_msg->frame_type.isKeyFrame()) {
        if (video_msg->page_list.first == client_session->paused_keyframe_page)
            return Result::Success;

        if (!client_session->resumed) {
            if (client_session->paused_keyframe_page) {
                assert (client_session->paused_keyframe_page_pool);
                client_session->paused_keyframe_page_pool->pageUnref (client_session->paused_keyframe_page);
                client_session->paused_keyframe_page = NULL;
                client_session->paused_keyframe_page_pool = NULL;
            }

            client_session->paused_keyframe_page_pool = video_msg->page_pool;
            client_session->paused_keyframe_page = video_msg->page_list.first;
            video_msg->page_pool->pageRef (video_msg->page_list.first);
        }

//#warning ERROR This is LIKELY WRONG, breaks keyframe awaiting
        client_session->first_keyframe_sent = true;
        client_session->keyframe_sent = true;
        client_session->no_keyframe_counter = 0;
    }

//#warning TODO Set the same timestamp for prepush video messages (last video msg timestamp?).

//    VideoMessage tmp_video_msg = *video_msg;
//    tmp_video_msg.timestamp_nanosec = 0;
//    client_session->rtmp_conn->sendVideoMessage (&tmp_video_msg);
    client_session->rtmp_conn->sendVideoMessage (video_msg);
    return Result::Success;
}

namespace {
struct StartWatchingCallback_Data : public Referenced
{
    MomentRtmpModule       *moment_rtmp_module;
    WeakRef<ClientSession>  weak_client_session;
    StRef<String>           stream_name;
    Uint32                  msg_stream_id;
};
}

static void startWatchingCallback (MediaSource                  * const media_source,
                                   ClientManager::ClientSession * const /* cm_session */,
                                   void                         * const _data)
{
    StartWatchingCallback_Data * const data = static_cast <StartWatchingCallback_Data*> (_data);
    MomentRtmpModule * const self = data->moment_rtmp_module;

    Ref<ClientSession> const client_session = data->weak_client_session.getRef ();
    if (!client_session) {
        logD (mod_rtmp, _func, "client session gone");
        return;
    }

    logD_ (_func, "client_session 0x", fmt_hex, (UintPtr) client_session.ptr());

    if (!media_source) {
        logD_ (_func, "video stream not found: ", data->stream_name);
        client_session->rtmp_conn->reportError ();
        return;
    }

    if (!client_session) {
//#warning TODO StreamManager::decStreamuseCount()
//        MomentServer::getInstance()->decStreamUseCount (stream);
        logD_ (_func, "client session gone");
        return;
    }

    if (!self->completeStartWatching (media_source, client_session, data->stream_name->mem(), data->msg_stream_id)) {
        client_session->rtmp_conn->reportError ();
        return;
    }
}

static void clientSessionDeletionCallback (void * const _stream)
{
    VideoStream * const stream = static_cast <VideoStream*> (_stream);
    (void) stream;

//    MomentServer * const moment = MomentServer::getInstance();
//#warning TODO StreamManager::decStreamuseCount()
//    moment->decStreamUseCount (stream);
}

Result
MomentRtmpModule::completeStartWatching (MediaSource   * const mt_nonnull media_source,
                                         ClientSession * const mt_nonnull client_session,
                                         ConstMemory     const stream_name,
                                         Uint32          const msg_stream_id)
{
    if (media_source->type == MediaSource::Type::MediaStream) {
        MediaStream * const stream = static_cast <MediaStream*> (media_source);
        return completeStartWatching_MediaStream (stream, client_session, stream_name, msg_stream_id);
    }

    assert (media_source->type == MediaSource::Type::VodSource);
    VodSource * const vod_source = static_cast <VodSource*> (media_source);
    return completeStartWatching_VodSource (vod_source, client_session, stream_name, msg_stream_id);
}

Result
MomentRtmpModule::completeStartWatching_MediaStream (MediaStream   * const mt_nonnull stream,
                                                     ClientSession * const mt_nonnull client_session,
                                                     ConstMemory     const stream_name,
                                                     Uint32          const msg_stream_id)
{
    logD_ (_func, "client_session 0x", fmt_hex, (UintPtr) client_session);

    client_session->addDeletionCallback (
            CbDesc<Object::DeletionCallback> (clientSessionDeletionCallback, stream, stream));

    client_session->mutex.lock ();
    // TODO Set watching_media_source to NULL when it's not needed anymore.
    client_session->watching_media_source = stream;

    stream->lock ();
    if (stream->isClosed_locked()) {
        stream->unlock ();
        client_session->mutex.unlock ();

        logD_ (_func, "video stream closed: ", stream_name);
        return Result::Failure;
    }

    bool const publishing = stream->isPublishingInProgress_locked();
    client_session->is_publishing_in_progress = publishing;

//#warning TODO Когда create_on_play выключен, отправлять стартовые сообщения сразу?
    if (publishing) {
        if (!sendStreamStartMessages (client_session->rtmp_conn,
                                      msg_stream_id,
                                      stream_name,
                                      false /* is_vod */))
        {
            client_session->mutex.unlock ();
            stream->unlock ();
            logE_ (_func, "sendStreamStartMessages() failed");
            return Result::Failure;
        }

//#warning TODO Send onMetaData in all control paths.
        if (!RtmpUtil::sendMetaData (client_session->rtmp_conn,
                                     msg_stream_id,
                                     stream->getFrameSaver()->getMediaDesc_Audio(),
                                     stream->getFrameSaver()->getMediaDesc_Video()))
        {
            client_session->mutex.unlock ();
            stream->unlock ();
            logE_ (_func, "sendMetaData() failed");
            return Result::Failure;
        }
    } else {
        if (!RtmpUtil::sendNetStreamPlayStreamNotFound (client_session->rtmp_conn, msg_stream_id, stream_name)) {
            client_session->mutex.unlock ();
            stream->unlock ();
            logE_ (_func, "sendNetStreamPlayStreamNotFound() failed");
            return Result::Failure;
        }
    }

//#warning ERROR I REALLY don't like the complexity of saved frames handling in mod_rtmp. Re-visit the whole thing.
    stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, client_session);
    client_session->mutex.unlock ();

    logD_ (_func, "session 0x", fmt_hex, (UintPtr) client_session, ": "
           "subscribing for out_stream_events, stream 0x", fmt_hex, (UintPtr) stream);

    stream->getEventInformer()->subscribe_locked (
            CbDesc<MediaStream::Events> (
                    &out_stream_events, client_session, client_session));

    stream->getStateEventInformer()->subscribe_locked (
            CbDesc<MediaStream::StateEvents> (
                    &out_stream_state_events, client_session, client_session));

    mt_unlocks (stream->mutex) stream->plusOneWatcher_locked (client_session /* guard_obj */);

    return Result::Success;
}

static void vod_mediaMessage (MediaMessage * mt_nonnull msg,
                              void         *cb_data);

static void vod_endOfSource (VodSource::EndReason  end_reason,
                             void                 *cb_data);

static VodSource::Frontend const vod_source_frontend = {
    vod_mediaMessage,
    vod_endOfSource
};

Result
MomentRtmpModule::completeStartWatching_VodSource (VodSource     * const mt_nonnull vod_source,
                                                   ClientSession * const mt_nonnull client_session,
                                                   ConstMemory     const stream_name,
                                                   Uint32          const msg_stream_id)
{
    logD_ (_func, "client_session 0x", fmt_hex, (UintPtr) client_session);

    vod_source->setFrontend (
            CbDesc<VodSource::Frontend> (
                    &vod_source_frontend, client_session, client_session));

    client_session->mutex.lock ();

    // TODO Set watching_media_source to NULL when it's not needed anymore.
    client_session->watching_media_source = vod_source;
    client_session->vod_mode = true;

    if (client_session->watching_params.got_start_time)
        vod_source->seek (client_session->watching_params.start_time_millisec * 1000);

    if (client_session->watching_params.got_duration) {
        vod_source->start (true /* stop_time_enabled */,
                           1000 * (  client_session->watching_params.start_time_millisec
                                   + client_session->watching_params.duration_millisec));
    } else {
        vod_source->start (false /* stop_time_enabled */,
                           0     /* stop_time_microsec */);
    }

    client_session->vod_source_started = true;

    client_session->mutex.unlock ();

    if (!sendStreamStartMessages (client_session->rtmp_conn,
                                  msg_stream_id,
                                  stream_name,
                                  true /* is_vod */))
    {
        logE_ (_func, "sendStreamStartMessages() failed");
        return Result::Failure;
    }

    return Result::Success;
}

static void vod_senderStateChanged (SenderState     const sender_state,
                                    ClientSession * const mt_nonnull client_session)
{
    logD_ (_func, "sender_state ", sender_state);

    assert (client_session->vod_mode);
    assert (client_session->watching_media_source);
    VodSource * const vod_source = static_cast <VodSource*> (client_session->watching_media_source.ptr());

    switch (sender_state) {
        case SenderState::ConnectionReady: {
            logD_ (_func, "resume");
            client_session->mutex.lock ();
            client_session->vod_sender_blocked = false;
            if (!client_session->vod_ratelimit_blocked) {
                vod_source->resume ();
            }
            client_session->mutex.unlock ();
        } break;
        case SenderState::ConnectionOverloaded: {
            // We used to set 'client_session->overloaded' to 'true' here,
            // but this turned out to happen too frequently.
            // Moved that to QueueSoftLimit instead.
        } break;
        case SenderState::QueueSoftLimit:
        case SenderState::QueueHardLimit: {
            logD_ (_func, "block");
//#warning TODO sender state notifications are deferred. We should check sender state immediately after send*() and block vod_source if needed.
            vod_source->block ();
            client_session->rtmp_conn->getSender()->flush ();
        } break;
    }
}

// Note that RtmpConnection state is also locked.
static mt_mutex (Sender::mutex) void
vod_senderStateCallback (SenderState   const sender_state,
                         void        * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    assert (client_session->vod_mode);
    assert (client_session->watching_media_source);
    VodSource * const vod_source = static_cast <VodSource*> (client_session->watching_media_source.ptr());

    switch (sender_state) {
        case SenderState::ConnectionReady: {
            // No-op
        } break;
        case SenderState::ConnectionOverloaded: {
            // We used to set 'client_session->overloaded' to 'true' here,
            // but this turned out to happen too frequently.
            // Moved that to QueueSoftLimit instead.
        } break;
        case SenderState::QueueSoftLimit:
        case SenderState::QueueHardLimit: {
            logD_ (_func, "block");
            vod_source->block ();
            client_session->rtmp_conn->getSender()->flush_locked ();
        } break;
    }
}

static Size sendAudioMediaDesc (ClientSession   * const mt_nonnull client_session,
                                MediaDesc_Audio * const mt_nonnull media_desc__audio)
{
    if (media_desc__audio->getType() != MediaDesc_Audio::AAC)
        return 0;

    MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (media_desc__audio);
    ConstMemory const codec_data_mem = media_desc__aac->getCodecData();

    AudioMessage audio_msg;
    audio_msg.pts_nanosec = client_session->last_vod_audio_timestamp_nanosec;
    audio_msg.dts_nanosec = audio_msg.pts_nanosec;

    audio_msg.page_pool = client_session->page_pool;
    client_session->page_pool->getFillPages (&audio_msg.page_list, codec_data_mem);
    audio_msg.msg_len = codec_data_mem.len();
    audio_msg.msg_offset = 0;

    audio_msg.frame_type = AudioFrameType::AacSequenceHeader;
    audio_msg.codec_id   = AudioCodecId::AAC;
    audio_msg.rate       = media_desc__audio->rate;
    audio_msg.channels   = media_desc__audio->channels;

    client_session->rtmp_conn->sendAudioMessage (&audio_msg, vod_senderStateCallback, client_session);
    audio_msg.release ();

    return audio_msg.msg_len;
}

static Size sendVideoMediaDesc (ClientSession   * const mt_nonnull client_session,
                                MediaDesc_Video * const mt_nonnull media_desc__video)
{
    if (media_desc__video->getType() != MediaDesc_Video::AVC)
        return 0;

    MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (media_desc__video);
    ConstMemory const codec_data_mem = media_desc__avc->getCodecData();

    VideoMessage video_msg;
    video_msg.pts_nanosec = client_session->last_vod_video_timestamp_nanosec;
    video_msg.dts_nanosec = video_msg.pts_nanosec;

    video_msg.page_pool = client_session->page_pool;
    client_session->page_pool->getFillPages (&video_msg.page_list, codec_data_mem);
    video_msg.msg_len = codec_data_mem.len();
    video_msg.msg_offset = 0;

    video_msg.frame_type = VideoFrameType::AvcSequenceHeader;
    video_msg.codec_id   = VideoCodecId::AVC;

    client_session->rtmp_conn->sendVideoMessage (&video_msg, vod_senderStateCallback, client_session);
    video_msg.release ();

    return video_msg.msg_len;
}

static void rateLimitTimerTick (void *_client_session);

static void vod_mediaMessage (MediaMessage * const mt_nonnull msg,
                              void         * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    logS_ (_func, *msg);

    Size msg_len = 0;
    bool has_timestamp = false;
    switch (msg->msg_type) {
        case MediaMessage::Type_Audio: {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);
            client_session->last_vod_audio_timestamp_nanosec = audio_msg->pts_nanosec;

            client_session->rtmp_conn->sendAudioMessage (audio_msg, vod_senderStateCallback, client_session);

            msg_len = audio_msg->msg_len;
            has_timestamp = audio_msg->frame_type.hasTimestamp();
        } break;
        case MediaMessage::Type_Video: {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
            client_session->last_vod_video_timestamp_nanosec = video_msg->pts_nanosec;

//            logD_ (_func, "video_msg:");
//            PagePool::dumpPages (logs, &video_msg->page_list, video_msg->msg_offset);

            client_session->rtmp_conn->sendVideoMessage (video_msg, vod_senderStateCallback, client_session);

            msg_len = video_msg->msg_len;
            has_timestamp = video_msg->frame_type.hasTimestamp();
        } break;
        case MediaMessage::Type_MediaDesc: {
            MediaDescMessage * const media_desc_msg = static_cast <MediaDescMessage*> (msg);
            if (media_desc_msg->media_desc__audio) {
                msg_len = sendAudioMediaDesc (client_session, media_desc_msg->media_desc__audio);
            }
            if (media_desc_msg->media_desc__video) {
                msg_len = sendVideoMediaDesc (client_session, media_desc_msg->media_desc__video);
            }
        } break;
        default: {
            logD_ (_func, "ignoring media message ", *msg);
            return;
        } break;
    }

    Uint64 const wait_microsec =
            client_session->vod_ratelimit.frame (getTimeMicroseconds(),
                                                 has_timestamp,
                                                 msg->dts_nanosec / 1000,
                                                 msg_len);
    if (wait_microsec > 0) {
        logD_ (_funcp (client_session), "wait_microsec ", wait_microsec);

        assert (client_session->watching_media_source
                && client_session->watching_media_source->type == MediaSource::Type::VodSource);
        VodSource * const vod_source = static_cast <VodSource*> (client_session->watching_media_source.ptr());

        client_session->mutex.lock ();
        if (client_session->vod_ratelimit_blocked) {
            logW_ (_funcp (client_session), "ratelimit_blocked");
        } else {
            client_session->vod_ratelimit_blocked = true;
            vod_source->block ();
            client_session->rtmp_conn->getTimers()->addTimer_microseconds (
                    CbDesc<Timers::TimerCallback> (&rateLimitTimerTick, client_session, client_session),
                    wait_microsec,
                    false /* periodical */,
                    true  /* auto_delete */,
                    true  /* delete_after_tick */);
        }
        client_session->mutex.unlock ();
    }
}

static void rateLimitTimerTick (void * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    logD_ (_funcp (client_session));

    assert (client_session->watching_media_source
            && client_session->watching_media_source->type == MediaSource::Type::VodSource);
    VodSource * const vod_source = static_cast <VodSource*> (client_session->watching_media_source.ptr());

    client_session->mutex.lock ();
    client_session->vod_ratelimit_blocked = false;
    if (!client_session->vod_sender_blocked) {
        vod_source->resume ();
    }
    client_session->mutex.unlock ();
}

static void vod_endOfSource (VodSource::EndReason   const end_reason,
                             void                 * const /* _client_session */)
{
    logD_ (_func, "end_reason ", end_reason);

  // TODO Do something (send eof/disconnect)
}

Result in_audioMessage (AudioMessage * const mt_nonnull msg,
                        void         * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // We create 'video_stream' in startStreaming()/startWatching(), which is
    // synchronized with autioMessage(). No locking needed.
    if (client_session->video_stream)
        client_session->video_stream->fireAudioMessage (msg);

    return Result::Success;
}

Result in_videoMessage (VideoMessage * const mt_nonnull msg,
                        void         * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    // We create 'video_stream' in startStreaming()/startWatching(), which is
    // synchronized with videoMessage(). No locking needed.
    if (client_session->video_stream)
        client_session->video_stream->fireVideoMessage (msg);

    return Result::Success;
}

static Result doConnect (ClientSession * const mt_nonnull client_session,
                         Uint32          const msg_stream_id,
                         AmfDecoder    * const mt_nonnull decoder)
{
    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->beginObject ()) {
	logE_ (_func, "no command object");
	return Result::Failure;
    }

    Byte app_name_buf [1024];
    Size app_name_len = 0;

    Byte page_url_buf [1024];
    Size page_url_len = 0;

    double object_encoding = 0.0;

    while (!decoder->isObjectEnd()) {
	Byte field_name_buf [512];
	Size field_name_len;
	Size field_name_full_len;
	if (!decoder->decodeFieldName (Memory::forObject (field_name_buf), &field_name_len, &field_name_full_len)) {
	    logE_ (_func, "no \"app\" field in the command object");
	    return Result::Failure;
	}

        ConstMemory const field_name_mem (field_name_buf, field_name_len);
        logD_ (_func, "field name: ", field_name_mem);

	if (equal (field_name_mem, "app")) {
	    Size app_name_full_len;
	    if (!decoder->decodeString (Memory::forObject (app_name_buf), &app_name_len, &app_name_full_len)) {
		logE_ (_func, "could not decode app name");
		return Result::Failure;
	    } else {
                if (app_name_full_len > app_name_len) {
                    logW_ (_func, "app name length exceeds limit: "
                           "length ", app_name_full_len, " bytes, limit ", sizeof (app_name_buf), " bytes");
                }
            }

            logD_ (_func, "app_name_full_len ", app_name_full_len);
	} else
        if (equal (field_name_mem, "objectEncoding")) {
            double number;
            if (!decoder->decodeNumber (&number)) {
                logE_ (_func, "could not decode objectEncoding");
                return Result::Failure;
            }
            object_encoding = number;
        } else
        if (equal (field_name_mem, "pageUrl")) {
            Size page_url_full_len;
            if (!decoder->decodeString (Memory::forObject (page_url_buf), &page_url_len, &page_url_full_len)) {
                logW_ (_func, "could not decode pageUrl");
            } else {
                if (page_url_full_len > page_url_len) {
                    logW_ (_func, "pageUrl length exceeds limit: "
                           "length ", page_url_full_len, " bytes, limit ", sizeof (page_url_buf), " bytes");
                }
            }
        } else
	if (!decoder->skipValue ()) {
	    logE_ (_func, "could not skip field value");
	    return Result::Failure;
	}
    }

    {
        client_session->mutex.lock ();

        if (page_url_len)
            client_session->referer_page_url = newString (ConstMemory (page_url_buf, page_url_len));

        if (app_name_len)
            client_session->app_name = newString (ConstMemory (app_name_buf, app_name_len));

        logD_ (_func, "app_name \"", ConstMemory (app_name_buf, app_name_len), "\"");

        client_session->mutex.unlock ();
    }

    client_session->rtmp_conn->sendWindowAckSize (client_session->rtmp_conn->getLocalWackSize());
    client_session->rtmp_conn->sendSetPeerBandwidth (client_session->rtmp_conn->getRemoteWackSize(), 2 /* dynamic limit */);
    client_session->rtmp_conn->sendUserControl_StreamBegin (RtmpConnection::CommandMessageStreamId);

//    if (rtmp_sample_access)
//        sendRtmpSampleAccess (client_session->rtmp_conn, RtmpConnection::DefaultMessageStreamId, true, true);

    {
	AmfAtom atoms [27];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("fmsVer");
            // TODO FMLE doesn't allow DVR if we don't pretend to be FMS>=3.5.
            //      Make this configurable and default to 'FMS' when rtmp recording
            //      is enabled.
//	    encoder.addString ("MMNT/0,1,0,0");
	    encoder.addString ("FMS/3,5,7,7009");

	    encoder.addFieldName ("capabilities");
	    // TODO Define capabilities. Docs?
	    encoder.addNumber (31.0);

	    encoder.addFieldName ("mode");
	    encoder.addNumber (1.0);

	    encoder.endObject ();
	}

	{
	    encoder.beginObject ();

	    encoder.addFieldName ("level");
	    encoder.addString ("status");

	    encoder.addFieldName ("code");
	    encoder.addString ("NetConnection.Connect.Success");

	    encoder.addFieldName ("description");
	    encoder.addString ("Connection succeeded.");

	    encoder.addFieldName ("data");
	    encoder.beginEcmaArray (0);

	    encoder.addFieldName ("version");
	    encoder.addString ("3,5,7,7009");

	    encoder.endEcmaArray ();

//#warning TODO generate clientid values
            encoder.addFieldName ("clientid");
            encoder.addNumber (1.0);

	    encoder.addFieldName ("objectEncoding");
	    encoder.addNumber (object_encoding);

	    encoder.endObject ();
	}

	Byte msg_buf [1024];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "encode() failed");
	    return Result::Failure;
	}

	client_session->rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    return Result::Success;
}

Result
MomentRtmpModule::doPlay (ClientSession * const mt_nonnull client_session,
                          Uint32          const msg_stream_id,
                          AmfDecoder    * const mt_nonnull decoder)
{
  // Sending "_result" response to a "play" request is not necessary.

    logD (session, _func_);

    if (client_session->playing.get()) {
	logW_ (_func, "already playing");
	return Result::Success;
    }
    client_session->playing.set (1);

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    Byte vs_name_buf [512];
    Size vs_name_len;
    Size vs_name_full_len;
    if (!decoder->decodeString (Memory::forObject (vs_name_buf), &vs_name_len, &vs_name_full_len)) {
	logE_ (_func, "could not decode video stream name");
	return Result::Failure;
    }
    if (vs_name_full_len > vs_name_len) {
	logW_ (_func, "video stream name length exceeds limit "
	       "(length ", vs_name_full_len, " bytes, limit ", sizeof (vs_name_buf), " bytes)");
    }

    ConstMemory stream_name (vs_name_buf, vs_name_len);

    if (client_session->watching) {
	logE (mod_rtmp, _func, "already watching another stream");
        return Result::Success;
    }
    client_session->watching = true;

    ConstMemory stream_name_with_params = stream_name;

    client_session->mutex.lock ();
    if (!client_session->valid) {
        client_session->mutex.unlock ();
        return Result::Failure;
    }

    resetWatchingParams (&client_session->watching_params);
    {
	Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
	if (name_sep) {
            parseHttpParameters_noDecode (stream_name.region (name_sep + 1 - stream_name.mem()),
                                          startRtmpWatching_paramCallback,
                                          &client_session->watching_params);
	    stream_name = stream_name.region (0, name_sep - stream_name.mem());
	}
    }

    StRef<String> const referer_page_url = newString (client_session->referer_page_url);

    StRef<String> app_name;
    {
        ConstMemory app_name_mem = client_session->app_name;
        if (trim_app_name) {
            Byte const * const slash_pos = (Byte const *) memchr (app_name_mem.buf(), '/', app_name_mem.len());
            if (slash_pos)
                app_name_mem = app_name_mem.region (0, slash_pos - app_name_mem.buf());
        }
        app_name = newString (app_name_mem);
    }

    client_session->stream_name = makeString (app_name, "/", stream_name);
    client_session->resumed = !client_session->watching_params.start_paused;
    client_session->mutex.unlock ();

    Ref<MediaSource> media_source;
    {
        Ref<StartWatchingCallback_Data> const data = grabNew <StartWatchingCallback_Data> ();
        data->moment_rtmp_module = this;
        data->weak_client_session = client_session;
        data->stream_name = makeString (app_name, "/", stream_name);
        data->msg_stream_id = msg_stream_id;

        ClientManager::ClientRequestInfo req_info;
        req_info.stream_link = stream_name_with_params;
        req_info.stream_name = data->stream_name;
        req_info.client_addr = client_session->client_addr;
        req_info.referer_page_url = referer_page_url;
        req_info.module_name = "mod_rtmp";
        req_info.protocol_name = "rtmp"; // TODO "rtmpt?"
        req_info.auth_id = (client_session->watching_params.auth_key ?
                                   client_session->watching_params.auth_key->mem() :
                                   ConstMemory());
        req_info.rec_mode = RecordingMode::NoRecording;

        Ref<ClientManager::ClientSession> cm_session;
        bool const complete =
                client_manager->startWatching (
                        &req_info,
                        true  /* enable_vod */,
                        // TODO client_session_backend
                        CbDesc<ClientManager::ClientSessionBackend> (),
                        client_session,
                        CbDesc<ClientManager::StartWatchingCallback> (
                                startWatchingCallback, data, this, data),
                        &media_source,
                        &cm_session);
        if (!complete)
            return Result::Success;

        logD_ (_func, "client_manager.startWatching() completed immediately");
    }

    if (!media_source) {
        logD (mod_rtmp, _func, "media source not found: ", stream_name);
        return Result::Failure;
    }

    return completeStartWatching (media_source, client_session, stream_name, msg_stream_id);
}

static Result doPause (ClientSession * const mt_nonnull client_session,
                       Uint32          const msg_stream_id,
		       AmfDecoder    * const mt_nonnull decoder)
{
    logD (session, _func_);

    if (!client_session->playing.get()) {
	logW_ (_func, "not playing");
	return Result::Success;
    }

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    bool is_pause;
    if (!decoder->decodeBoolean (&is_pause)) {
	logE_ (_func, "could not decode boolean");
	return Result::Failure;
    }

    {
	AmfAtom atoms [4];
	AmfEncoder encoder (atoms);

	encoder.addString ("_result");
	encoder.addNumber (transaction_id);
	encoder.addNullObject ();

	Byte msg_buf [512];
	Size msg_len;
	if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	    logE_ (_func, "could not encode reply");
	    return Result::Failure;
	}

	client_session->rtmp_conn->sendCommandMessage_AMF0 (msg_stream_id, ConstMemory (msg_buf, msg_len));
    }

    if (is_pause) {
        /* No-op */
    } else {
        client_session->doResume ();
    }

    return Result::Success;
}

Result
MomentRtmpModule::doPublish (ClientSession * const mt_nonnull client_session,
                             Uint32          const msg_stream_id,
                             AmfDecoder    * const mt_nonnull decoder,
                             RtmpConnection::ConnectionInfo * const mt_nonnull conn_info)
{
    logD (session, _func_);

    double transaction_id;
    if (!decoder->decodeNumber (&transaction_id)) {
	logE_ (_func, "could not decode transaction_id");
	return Result::Failure;
    }

    if (!decoder->skipObject ()) {
	logE_ (_func, "could not skip command object");
	return Result::Failure;
    }

    Byte vs_name_buf [512];
    Size vs_name_len;
    Size vs_name_full_len;
    if (!decoder->decodeString (Memory::forObject (vs_name_buf), &vs_name_len, &vs_name_full_len)) {
	logE_ (_func, "could not decode video stream name");
	return Result::Failure;
    }
    if (vs_name_full_len > vs_name_len) {
	logW_ (_func, "video stream name length exceeds limit "
	       "(length ", vs_name_full_len, " bytes, limit ", sizeof (vs_name_buf), " bytes)");
    }

    ConstMemory stream_name (vs_name_buf, vs_name_len);

    RecordingMode rec_mode = RecordingMode::NoRecording;
    {
	Byte rec_mode_buf [512];
	Size rec_mode_len;
	Size rec_mode_full_len;
	if (decoder->decodeString (Memory::forObject (rec_mode_buf), &rec_mode_len, &rec_mode_full_len)) {
	    if (rec_mode_full_len > rec_mode_len) {
		logW_ (_func, "recording mode length exceeds limit "
		       "(length ", rec_mode_full_len, " bytes, limit ", sizeof (rec_mode_buf), " bytes)");
	    } else {
		ConstMemory const rec_mode_mem (rec_mode_buf, rec_mode_len);
		if (equal (rec_mode_mem, "live"))
		    rec_mode = RecordingMode::NoRecording;
		else
		if (equal (rec_mode_mem, "record"))
		    rec_mode = RecordingMode::Replace;
		else
		if (equal (rec_mode_mem, "append"))
		    rec_mode = RecordingMode::Append;
		else
		    logW_ (_func, "unknown recording mode: ", rec_mode_mem);

                logD_ (_func, "rec_mode: ", rec_mode_mem, " (", rec_mode, ")");
	    }
	} else {
	    logD_ (_func, "could not decode recording mode");
	}
    }

    if (client_session->streaming) {
	logE (mod_rtmp, _func, "already streaming another stream");
        return Result::Success;
    }
    client_session->streaming = true;

    Ref<StreamParameters> const stream_params = grab (new (std::nothrow) StreamParameters);
    stream_params->setParam ("source", conn_info->momentrtmp_proto ? ConstMemory ("momentrtmp") : ConstMemory ("rtmp"));
    if (!conn_info->momentrtmp_proto) {
        // TODO nellymoser? Use "source" param from above instead.
        stream_params->setParam ("audio_codec", "speex");
    }

    Ref<MediaStream> const stream =
            grabNewObject <MediaStream> (
                    moment_server->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());
    stream->setStreamParameters (stream_params);
    stream->init_setPublishingInProgress (true);

    logD_ (_func, "client_session: 0x", fmt_hex, (UintPtr) client_session, ", "
           "stream: 0x", (UintPtr) stream.ptr());

    mutex.lock ();
    client_session->mutex.lock ();

    if (!client_session->valid) {
        mutex.unlock ();
        client_session->mutex.unlock ();
        return Result::Failure;
    }

    if (!client_session->streaming_hash_entry) {
        assert (!client_session->streaming_list_el);

        StreamHashEntry *entry = streaming_hash.lookup (stream_name);
        if (!entry) {
            entry = new (std::nothrow) StreamHashEntry;
            entry->stream_name = st_grab (new (std::nothrow) String (stream_name));
            streaming_hash.add (entry);
        }

        client_session->streaming_hash_entry = entry;
        client_session->streaming_list_el = entry->session_list.append (client_session);
    } else {
        assert (client_session->streaming_list_el);
        logD_ (_func, "session already in streaming hash");
    }

    mutex.unlock ();

    client_session->video_stream = stream;
    client_session->streaming_params.reset ();
    {
	Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
	if (name_sep) {
            ConstMemory const params_mem = stream_name.region (name_sep + 1 - stream_name.mem());
            logD_ (_func, "parameters: ", params_mem);
            parseHttpParameters_noDecode (params_mem,
                                          startRtmpStreaming_paramCallback,
                                          &client_session->streaming_params);
	    stream_name = stream_name.region (0, name_sep - stream_name.mem());
	}
    }
    client_session->stream_name = newString (stream_name);

    StRef<String> const referer_page_url = newString (client_session->referer_page_url);
    client_session->mutex.unlock ();

    Result res = Result::Failure;
    {
        Ref<StartStreamingCallback_Data> const data = grab (new (std::nothrow) StartStreamingCallback_Data);
        data->moment_rtmp_module = this;
        data->weak_client_session = client_session;
        data->stream_name = st_grab (new (std::nothrow) String (stream_name));
        data->rec_mode = rec_mode;
        data->video_stream = stream;
        data->msg_stream_id = msg_stream_id;
        data->transaction_id = transaction_id;

        ClientManager::ClientRequestInfo req_info;
        req_info.stream_link = stream_name;
        req_info.stream_name = stream_name;
        req_info.client_addr = client_session->client_addr;
        req_info.referer_page_url = referer_page_url;
        req_info.module_name = "mod_rtmp";
        req_info.protocol_name = "rtmp"; // TODO "rtmpt?"
        req_info.auth_id = (client_session->streaming_params.auth_key ?
                                   client_session->streaming_params.auth_key->mem() :
                                   ConstMemory());
        req_info.rec_mode = rec_mode;

        Ref<ClientManager::ClientSession> cm_session;
        bool const complete =
                client_manager->startStreaming (
                        &req_info,
                        stream,
                        // TODO client_session_backend
                        CbDesc<ClientManager::ClientSessionBackend> (),
                        client_session,
                        CbDesc<ClientManager::StartStreamingCallback> (
                                startStreamingCallback, data, this, data),
                        &res,
                        &cm_session);
        if (!complete)
            return Result::Success;
    }

    if (!res)
        return Result::Failure;

    return completeStartStreaming (client_session, stream_name, rec_mode, stream, msg_stream_id, transaction_id);
}

static Result in_commandMessage (bool                             const is_data,
                                 MediaMessage                   * const mt_nonnull msg,
                                 Uint32                           const msg_stream_id,
                                 AmfEncoding                      const amf_encoding,
                                 RtmpConnection::ConnectionInfo * const mt_nonnull conn_info,
                                 void                           * const _client_session)
{
    logD (mod_rtmp, _func_);

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);
    Ref<MomentRtmpModule> const self = client_session->weak_rtmp_module.getRef ();
    if (!self)
        return Result::Success;

    if (msg->msg_len == 0)
        return Result::Success;

    if (amf_encoding == AmfEncoding::AMF3) {
        // AMF3 command messages have an extra leading (dummy?) byte.
        if (msg->msg_len < 1) {
            logW_ (_func, "AMF3 message is too short (no leading byte)");
            return Result::Success;
        }

        // empty message with a leading byte
        if (msg->msg_len == 1)
            return Result::Success;
    }

    Size const decoder_offset = (amf_encoding == AmfEncoding::AMF3 ? 1 : 0);
    PagePool::PageListArray pl_array (msg->page_list.first, decoder_offset, msg->msg_len - decoder_offset);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg->msg_len - decoder_offset);

    if (self->dump_amf && logLevelOn_ (LogLevel::Debug)) {
        logD_ (_func, "msg dump (", amf_encoding, "), msg_stream_id ", msg_stream_id, ":");
        decoder.dump ();
    }

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
                               &method_name_len,
                               NULL /* ret_full_len */))
    {
        logE_ (_func, "could not decode method name");
        return Result::Failure;
    }

    logD (session, _func, "method: ", ConstMemory (method_name, method_name_len));

    ConstMemory method_mem (method_name, method_name_len);
    if (!is_data) {
      // Each "if" branch must end with "return".
        if (equal (method_mem, "connect")) {
            return doConnect (client_session, msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "createStream")) {
            return client_session->rtmp_conn->doCreateStream (msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "FCPublish")) {
          // TEMPORAL TEST
            return client_session->rtmp_conn->doReleaseStream (msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "releaseStream")) {
            return client_session->rtmp_conn->doReleaseStream (msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "closeStream")) {
            return client_session->rtmp_conn->doCloseStream (msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "deleteStream")) {
            return client_session->rtmp_conn->doDeleteStream (msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "receiveVideo")) {
          // TODO
            return Result::Success;
        } else
        if (equal (method_mem, "receiveAudio")) {
          // TODO
            return Result::Success;
        } else
        if (equal (method_mem, "play")) {
            return self->doPlay (client_session, msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "pause")) {
            return doPause (client_session, msg_stream_id, &decoder);
        } else
        if (equal (method_mem, "resume")) {
          // TODO Unused, we never get here - ?
            client_session->doResume ();
            client_session->rtmp_conn->doBasicMessage (msg_stream_id, &decoder);
            return Result::Success;
        } else
        if (equal (method_mem, "publish")) {
            return self->doPublish (client_session, msg_stream_id, &decoder, conn_info);
        } else
        if (equal (method_mem, "@setDataFrame")) {
            Size const msg_offset = decoder.getCurOffset () + decoder_offset;
            assert (msg_offset <= msg->msg_len);

            VideoMessage video_msg;
            video_msg.pts_nanosec = msg->pts_nanosec;
            video_msg.dts_nanosec = msg->dts_nanosec;
            
            video_msg.frame_type = VideoFrameType::RtmpSetMetaData;
            video_msg.codec_id = VideoCodecId::Unknown;

            video_msg.page_pool = msg->page_pool;
            video_msg.page_list = msg->page_list;
            video_msg.msg_len = msg->msg_len - msg_offset;
            video_msg.msg_offset = msg_offset;

            return client_session->rtmp_conn->fireVideoMessage (&video_msg);
        } else
        if (equal (method_mem, "@clearDataFrame")) {
            Size const msg_offset = decoder.getCurOffset () + decoder_offset;
            assert (msg_offset <= msg->msg_len);

            VideoMessage video_msg;
            video_msg.pts_nanosec = msg->pts_nanosec;
            video_msg.dts_nanosec = msg->dts_nanosec;

            video_msg.frame_type = VideoFrameType::RtmpClearMetaData;
            video_msg.codec_id = VideoCodecId::Unknown;

            video_msg.page_pool = msg->page_pool;
            video_msg.page_list = msg->page_list;
            video_msg.msg_len = msg->msg_len - msg_offset;
            video_msg.msg_offset = msg_offset;

            return client_session->rtmp_conn->fireVideoMessage (&video_msg);
        }
    } else {
        if (client_session->video_stream) {
            logD (mod_rtmp, _func, "firing data message");
            client_session->video_stream->fireRtmpDataMessage (msg);
        }

        return Result::Success;
    }

    return Result::Success;
}

static void in_senderStateChanged (SenderState   const sender_state,
                                   void        * const _client_session)
{
    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    if (client_session->vod_mode) {
        vod_senderStateChanged (sender_state, client_session);
        return;
    }

    switch (sender_state) {
	case SenderState::ConnectionReady:
	    logD (framedrop, _func, "ConnectionReady");
          #ifdef MOMENT_RTMP__FLOW_CONTROL
	    client_session->mutex.lock ();
	    client_session->overloaded = false;
	    client_session->mutex.unlock ();
          #endif
	    break;
	case SenderState::ConnectionOverloaded:
	    logD (framedrop, _func, "ConnectionOverloaded");
            // We used to set 'client_session->overloaded' to 'true' here,
            // but this turned out to happen too frequently.
            // Moved that to QueueSoftLimit instead.
	    break;
	case SenderState::QueueSoftLimit:
	    logD (framedrop, _func, "QueueSoftLimit");
          #ifdef MOMENT_RTMP__FLOW_CONTROL
	    client_session->mutex.lock ();
	    client_session->overloaded = true;
	    client_session->mutex.unlock ();
          #endif
	    break;
	case SenderState::QueueHardLimit:
	    logE_ (_func, "QueueHardLimit");
            client_session->rtmp_conn->close ();
	    break;
	default:
	    unreachable();
    }
}

static void in_closed (Exception * const exc,
                       void      * const _client_session)
{
    logD (mod_rtmp, _func, "client_session 0x", fmt_hex, (UintPtr) _client_session);

    if (exc)
	logD (mod_rtmp, _func, exc->toString());

    ClientSession * const client_session = static_cast <ClientSession*> (_client_session);

    Ref<MomentRtmpModule> const self = client_session->weak_rtmp_module.getRef ();
    if (!self)
        return;

    self->destroyClientSession (client_session);
}

static RtmpConnection::Frontend const rtmp_frontend = {
    NULL /* handshakeComplete */,
    in_commandMessage,
    in_audioMessage,
    in_videoMessage,
    in_senderStateChanged,
    in_closed
};

static Result clientConnected (RtmpConnection * const mt_nonnull rtmp_conn,
                               IpAddress        const client_addr,
                               void           * const _self)
{
    MomentRtmpModule * const self = static_cast <MomentRtmpModule*> (_self);

    logD (mod_rtmp, _func_);

    Ref<ClientSession> const client_session = grabNewObject <ClientSession> (self->default_start_paused,
                                                                             self->page_pool,
                                                                             self->paused_avc_interframes);
    client_session->weak_rtmp_module = self;
    client_session->client_addr = client_addr;
    client_session->rtmp_conn = rtmp_conn;

    client_session->vod_ratelimit.init (self->ratelimit_params);

    {
	Ref<ServerThreadContext> thread_ctx =
		self->moment_server->getRecorderThreadPool()->grabThreadContext (
                        "flash" /* TODO Configurable prefix */,
                        /*guard_obj=*/ self);
	if (thread_ctx) {
	    logE_ (_func, "Couldn't get recorder thread context: ", exc->toString());
	    thread_ctx = self->moment_server->getServerApp()->getServerContext()->getMainThreadContext();
	}

        AvMuxer *muxer = NULL;
        if (self->record_mp4) {
            client_session->mp4_muxer.init (self->moment_server->getPagePool(), true /* derive_dts_from_pts */);
            muxer = &client_session->mp4_muxer;
        } else {
            client_session->flv_muxer.setPagePool (self->moment_server->getPagePool());
            muxer = &client_session->flv_muxer;
        }

	// TODO recorder frontend + error reporting
	client_session->recorder->init (CbDesc<AvRecorder::Frontend> (),
                                        thread_ctx,
                                        self->moment_server->getStorage(),
                                        muxer,
                                        self->recording_limit);
    }

    rtmp_conn->setFrontend (
            CbDesc<RtmpConnection::Frontend> (
                    &rtmp_frontend, client_session, client_session));

    rtmp_conn->startServer ();

    client_session->ref ();
    return Result::Success;
}

static RtmpVideoService::Frontend const rtmp_video_service_frontend = {
    clientConnected
};


// ___________________________________ Admin ___________________________________

HttpService::HttpHandler const MomentRtmpModule::admin_http_handler = {
    adminHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentRtmpModule::adminHttpRequest (HttpRequest  * const mt_nonnull req,
                                    HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
                                    IpAddress      const /* local_addr */,
                                    Sender       * const mt_nonnull conn_sender,
                                    Memory         const /* msg_body */,
                                    void        ** const mt_nonnull /* ret_msg_data */,
                                    void         * const _self)
{
    MomentRtmpModule * const self = static_cast <MomentRtmpModule*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "streaming_client_disconnect"))
    {
        ConstMemory const stream_name = req->getParameter ("stream");
        logD_ (_func, "client_disconnect: stream ", stream_name);

        {
            List< Ref<ClientSession> > session_list;

            self->mutex.lock ();
            if (StreamHashEntry * const entry = self->streaming_hash.lookup (stream_name)) {
                StreamList::iterator iter (entry->session_list);
                while (!iter.done()) {
                    ClientSession * const session = iter.next ()->data;
                    session_list.append (session);
                }
            }
            self->mutex.unlock ();

            {
                List< Ref<ClientSession> >::iterator iter (session_list);
                while (!iter.done()) {
                    ClientSession * const session = iter.next ()->data;
                    self->destroyClientSession (session);
                }
            }
        }

        ConstMemory const reply_mem = "OK";
        conn_sender->send (
                self->page_pool,
                true /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/plain", reply_mem.len()),
                "\r\n",
                reply_mem);
        logA_ ("mod_rtmp 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else
    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "client_stop_recording"))
    {
        ConstMemory const stream_name = req->getParameter ("stream");
        logD_ (_func, "client_stop_recording: stream ", stream_name);

        self->mutex.lock ();
        if (StreamHashEntry * const entry = self->streaming_hash.lookup (stream_name)) {
            StreamList::iterator iter (entry->session_list);
            while (!iter.done()) {
                ClientSession * const session = iter.next ()->data;
                session->recorder->stop ();
            }
        }
        self->mutex.unlock ();

        ConstMemory const reply_mem = "OK";
        conn_sender->send (
                self->page_pool,
                true /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/plain", reply_mem.len()),
                "\r\n",
                reply_mem);
        logA_ ("mod_rtmp 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else
    if (req->getNumPathElems() >= 2
	&& equal (req->getPath (1), "stat"))
    {
        Time const cur_unixtime = getUnixtime();

        PagePool::PageListInfo page_list;

        self->page_pool->printToPages (
                &page_list,
                "<html>"
                "<body>"
                "<p>mod_rtmp stats</p>");

        {
            self->page_pool->printToPages (
                    &page_list,
                    "<table>");

            self->rtmp_service->rtmpServiceLock ();

            RtmpService::SessionsInfo sinfo_sum;
            RtmpService::SessionInfoIterator iter = self->rtmp_service->getClientSessionsInfo_locked (&sinfo_sum);

            self->page_pool->printToPages (
                    &page_list,
                    "<tr><td>RtmpService: num_session_objects</td><td>", sinfo_sum.num_session_objects, "</td></tr>"
                    "<tr><td>RtmpService: num_valid_sessions</td><td>",  sinfo_sum.num_valid_sessions,  "</td></tr>"
                    "<tr><td>RtmpService sessions:</td></tr>");

            while (!iter.done()) {
                RtmpService::ClientSessionInfo * const sinfo = iter.next ();

                Byte time_buf [unixtimeToString_BufSize];
                Size const time_len = unixtimeToString (Memory::forObject (time_buf), sinfo->creation_unixtime);

                self->page_pool->printToPages (
                        &page_list,
                        "<tr>"
                        "<td>", sinfo->client_addr, "</td>"
                        "<td>", ConstMemory (time_buf, time_len), "</td>"
                        "<td>", sinfo->last_send_unixtime, "</td>"
                        "<td>", sinfo->last_recv_unixtime, "</td>"
                        "<td>", sinfo->last_play_stream, "</td>"
                        "<td>", sinfo->last_publish_stream, "</td>"
                        "</tr>");
            }

            self->rtmp_service->rtmpServiceUnlock ();

            self->page_pool->printToPages (
                    &page_list,
                    "</table>");
        }

        {
            self->rtmpt_service->rtmptServiceLock ();

            RtmptService::RtmptSessionsInfo sinfo_sum;
            RtmptService::RtmptSessionInfoIterator sinfo_iter = self->rtmpt_service->getRtmptSessionsInfo_locked (&sinfo_sum);

            RtmptService::RtmptConnectionsInfo cinfo_sum;
            RtmptService::RtmptConnectionInfoIterator cinfo_iter = self->rtmpt_service->getRtmptConnectionsInfo_locked (&cinfo_sum);

            self->page_pool->printToPages (
                    &page_list,
                    "<h1>RTMPT stats</h1>"
                    "<table>"
                    "<tr><td>RtmptService: num_session_objects</td><td>",    sinfo_sum.num_session_objects,    "</td></tr>"
                    "<tr><td>RtmptService: num_valid_sessions</td><td>",     sinfo_sum.num_valid_sessions,     "</td></tr>"
                    "<tr><td>RtmptService: num_connection_objects</td><td>", cinfo_sum.num_connection_objects, "</td></tr>"
                    "<tr><td>RtmptService: num_valid_connections</td><td>",  cinfo_sum.num_valid_connections,  "</td></tr>"
                    "</table>"
                    "<h2>RTMPT sessions</h2>"
                    "<table>");

            while (!sinfo_iter.done()) {
                RtmptService::RtmptSessionInfo * const sinfo = sinfo_iter.next ();

                Byte time_buf [unixtimeToString_BufSize];
                Size const time_len = unixtimeToString (Memory::forObject (time_buf), sinfo->creation_unixtime);

                Time idle_time = cur_unixtime - sinfo->last_req_unixtime;
                if (sinfo->last_req_unixtime > cur_unixtime)
                    idle_time = 0;

                self->page_pool->printToPages (
                        &page_list,
                        "<tr>"
                        "<td>", sinfo->last_client_addr, "</td>"
                        "<td>", ConstMemory (time_buf, time_len), "</td>"
                        "<td>", idle_time, "</td>"
                        "</tr>");
            }

            self->page_pool->printToPages (
                    &page_list,
                    "</table>"
                    "<h2>RTMPT connections</h2>"
                    "<table>");

            while (!cinfo_iter.done()) {
                RtmptService::RtmptConnectionInfo * const cinfo = cinfo_iter.next ();

                Byte time_buf [unixtimeToString_BufSize];
                Size const time_len = unixtimeToString (Memory::forObject (time_buf), cinfo->creation_unixtime);

                Time idle_time = cur_unixtime - cinfo->last_req_unixtime;
                if (cinfo->last_req_unixtime > cur_unixtime)
                    idle_time = 0;

                self->page_pool->printToPages (
                        &page_list,
                        "<tr>"
                        "<td>", cinfo->client_addr, "</td>"
                        "<td>", ConstMemory (time_buf, time_len), "</td>"
                        "<td>", idle_time, "</td>"
                        "</tr>");
            }

            self->page_pool->printToPages (
                    &page_list,
                    "</table>");

            self->rtmpt_service->rtmptServiceUnlock ();
        }

        self->page_pool->printToPages (
                &page_list,
                "</body>"
                "</html>");

        conn_sender->send (
                self->page_pool,
                false /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/html", page_list.data_len),
                "\r\n");
        conn_sender->sendPages (self->page_pool, page_list.first, /*msg_offs=*/ 0, page_list.data_len, true /* do_flush */);

        logA_ ("mod_rtmp 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
	logE_ (_func, "Unknown admin HTTP request: ", req->getFullPath());

	ConstMemory const reply_body = "mod_rtmp: unknown command";
	conn_sender->send (self->page_pool,
			   true /* do_flush */,
			   MOMENT_SERVER__404_HEADERS (reply_body.len()),
			   "\r\n",
			   reply_body);

	logA_ ("mod_rtmp 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}

// _____________________________________________________________________________

} // namespace {}
} // namespace Moment

namespace MomentRtmp {

Result momentRtmpInit (MomentInstance * const mt_nonnull moment_instance)
{
    Ref<MomentServer> const moment_server = moment_instance->getMomentServer();
    Ref<ServerApp> const server_app = moment_server->getServerApp();
    Ref<MConfig::Config> const config = moment_server->getConfigManager()->getConfig();

    {
        bool enable = true;

        ConstMemory const opt_name = "mod_rtmp/enable";
        if (!configGetBoolean (config, opt_name, &enable, enable, _func))
            return Result::Failure;

        if (!enable) {
            logI_ (_func, "RMTP module (mod_rtmp) is not enabled. "
                   "Set \"", opt_name, "\" option to \"y\" to enable.");
            return Result::Success;
        }
    }

    Ref<MomentRtmpModule> const rtmp_module = grabNewObject <MomentRtmpModule> ();
    if (!rtmp_module->init (moment_server)) {
        logE_ (_func, "rtmp_module.init() failed");
        return Result::Failure;
    }

    Uint64 send_delay_millisec = 50;
    if (!configGetUint64_ (config, "mod_rtmp/send_delay", &send_delay_millisec, _func))
        return Result::Failure;

    Time rtmp_ping_timeout_millisec;
    {
        Uint64 rtmp_ping_timeout_sec = 5 * 60;
        if (!configGetUint64_ (config, "mod_rtmp/ping_timeout", &rtmp_ping_timeout_sec, _func))
            return Result::Failure;

        rtmp_ping_timeout_millisec = rtmp_ping_timeout_sec * 1000;
    }

    Uint64 rtmpt_session_timeout = 30;
    if (!configGetUint64_ (config, "mod_rtmp/rtmpt_session_timeout", &rtmpt_session_timeout, _func))
        return Result::Failure;

    bool rtmpt_no_keepalive_conns = false;
    if (!configGetBoolean_ (config, "mod_rtmp/rtmpt_no_keepalive_conns", &rtmpt_no_keepalive_conns, _func))
        return Result::Failure;

    Time rtmp_accept_watchdog_timeout_sec = 0;
    {
	ConstMemory const opt_name = "mod_rtmp/rtmp_accept_watchdog_timeout";
	MConfig::GetResult const res = config->getUint64_default (
		opt_name, &rtmp_accept_watchdog_timeout_sec, rtmp_accept_watchdog_timeout_sec);
	if (!res)
	    logE_ (_func, "bad value for ", opt_name);

	logI_ (_func, opt_name, ": ", rtmp_accept_watchdog_timeout_sec);
    }

    {
	rtmp_module->rtmp_service->setFrontend (
                CbDesc<RtmpVideoService::Frontend> (
                        &rtmp_video_service_frontend, rtmp_module, rtmp_module));

	if (!rtmp_module->rtmp_service->init (server_app->getServerContext(),
                                              moment_server->getPagePool(),
                                              send_delay_millisec,
                                              rtmp_ping_timeout_millisec,
                                              rtmp_accept_watchdog_timeout_sec))
        {
	    logE_ (_func, "rtmp_service.init() failed: ", exc->toString());
	    return Result::Failure;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmp_bind";
	    ConstMemory rtmp_bind = config->getString_default (opt_name, ":1935");

	    logI_ (_func, opt_name, ": ", rtmp_bind);
	    if (!rtmp_bind.isNull ()) {
		IpAddress addr;
		if (!setIpAddress_default (rtmp_bind,
					   ConstMemory() /* default_host */,
					   1935          /* default_port */,
					   true          /* allow_any_host */,
					   &addr))
		{
		    logE_ (_func, "setIpAddress_default() failed (rtmp)");
		    return Result::Failure;
		}

		if (!rtmp_module->rtmp_service->bind (addr)) {
		    logE_ (_func, "rtmp_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmp_module->rtmp_service->start ()) {
		    logE_ (_func, "rtmp_service.start() failed: ", exc->toString());
		    return Result::Failure;
		}
	    } else {
		logI_ (_func, "RTMP service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to bind the service.");
	    }
	} while (0);
    }

    {
	ConstMemory const opt_name = "mod_rtmp/rtmpt_from_http";
	MConfig::BooleanValue const opt_val = config->getBoolean (opt_name);
	if (opt_val == MConfig::Boolean_Invalid)
	    logE_ (_func, "Invalid value for config option ", opt_name);
	else
	if (opt_val != MConfig::Boolean_False)
	    rtmp_module->rtmpt_service->attachToHttpService (moment_server->getHttpManager()->getHttpService());
    }

    {
	rtmp_module->rtmpt_service->setFrontend (
                CbDesc<RtmpVideoService::Frontend> (
                        &rtmp_video_service_frontend, rtmp_module, rtmp_module));

	if (!rtmp_module->rtmpt_service->init (
                                 server_app->getServerContext(),
                                 moment_server->getPagePool(),
                                 // TODO standalone_rtmpt enable/disable from config
                                 true /* enable_standalone_tcp_server */,
                                 rtmp_ping_timeout_millisec,
                                 rtmpt_session_timeout,
                                 // TODO Separate rtmpt_conn_keepalive_timeout
                                 rtmpt_session_timeout,
                                 rtmpt_no_keepalive_conns))
        {
	    logE_ (_func, "rtmpt_service.init() failed: ", exc->toString());
	    return Result::Failure;
	}

	do {
	    ConstMemory const opt_name = "mod_rtmp/rtmpt_bind";
	    ConstMemory const rtmpt_bind = config->getString_default (opt_name, ":8081");
	    logI_ (_func, opt_name, ": ", rtmpt_bind);
	    if (!rtmpt_bind.isNull ()) {
		IpAddress addr;
		if (!setIpAddress_default (rtmpt_bind,
					   ConstMemory() /* default_host */,
					   8081          /* default_port */,
					   true          /* allow_any_host */,
					   &addr))
		{
		    logE_ (_func, "setIpAddress_default() failed (rtmpt)");
		    return Result::Failure;
		}

		if (!rtmp_module->rtmpt_service->bind (addr)) {
		    logE_ (_func, "rtmpt_service.bind() failed: ", exc->toString());
		    break;
		}

		if (!rtmp_module->rtmpt_service->start ()) {
		    logE_ (_func, "rtmpt_service.start() failed: ", exc->toString());
		    return Result::Failure;
		}
	    } else {
              /*
		logI_ (_func, "RTMPT service is not bound to any port "
		       "and won't accept any connections. "
		       "Set \"", opt_name, "\" option to \"y\" to bind the service.");
               */
	    }
	} while (0);

        moment_server->getHttpManager()->getAdminHttpService()->addHttpHandler (
                CbDesc<HttpService::HttpHandler> (
                        &MomentRtmpModule::admin_http_handler, rtmp_module, rtmp_module),
                "mod_rtmp");
    }

    {
        Ref<RtmpPushProtocol> const rtmp_push_proto = grabNewObject <RtmpPushProtocol> ();
        rtmp_push_proto->init (moment_server, rtmp_ping_timeout_millisec);
        moment_server->getSourceManager()->addPushProtocol ("rtmp", rtmp_push_proto);
        moment_server->getSourceManager()->addPushProtocol ("momentrtmp", rtmp_push_proto);
    }
    {
        Ref<RtmpFetchProtocol> const rtmp_fetch_proto = grabNewObject <RtmpFetchProtocol> ();
        rtmp_fetch_proto->init (moment_server, rtmp_ping_timeout_millisec);
        moment_server->getSourceManager()->addFetchProtocol ("rtmp", rtmp_fetch_proto);
        moment_server->getSourceManager()->addFetchProtocol ("momentrtmp", rtmp_fetch_proto);
    }

    registerStatCounter (&stat_counter__rtmp_client_session);

    moment_instance->addDeletionData (rtmp_module);
    return Result::Success;
}

}

#ifndef MOMENT_STATIC_MODULES
  extern "C" bool
  libMary_moduleInit (void * const moment_instance_)
          { return MomentRtmp::momentRtmpInit (static_cast <MomentInstance*> (moment_instance_)); }
#endif

