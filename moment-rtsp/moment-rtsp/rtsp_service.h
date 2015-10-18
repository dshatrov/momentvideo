/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_RTSP__RTSP_SERVICE__H__
#define MOMENT_RTSP__RTSP_SERVICE__H__


#include <moment/libmoment.h>


namespace MomentRtsp {

using namespace Moment;

class RtspService : public Object
{
  private:
    StateMutex mutex;

    enum DataState {
        DataState_Header,
        DataState_Data
    };

    class RtpServerSockets : public Object
    {
      public:
        Embed<UdpSocket> rtp_socket;
        Embed<UdpSocket> rtcp_socket;

        RtpServerSockets (EmbedContainer *embed_container);
    };

    class TcpConnectionBlock : public Object
    {
      public:
        Embed<TcpConnection>             tcp_conn;
        Embed<ConnectionReceiver>        receiver;
        // TODO DeferredConnectionSender is preferred for tcp-interleaved mode,
        //      because it has better locking characteristics (less contention)
        //      and offloads data processing to client thread.
        Embed<ImmediateConnectionSender> sender;
        Embed<HttpServer>                http_server;

        TcpConnectionBlock (EmbedContainer *embed_container);
    };

    struct ReceiverMapping;

    class RtpDataSession : public Object
    {
      public:
        mt_const Uint32 ssrc;
        mt_const Uint32 freq;

        mt_const bool tcp_interleaved;
        mt_const Uint32 tcp_rtp_channel;
        mt_const Uint32 tcp_rtcp_channel;

        // Non-null when tcp_interleaved is true.
        mt_const Ref<TcpConnectionBlock> tcp_block;

        // Guarantees UdpSocket lifetime for senders.
        // Non-null when tcp_interleaved is false.
        mt_const Ref<RtpServerSockets> rtp_server_sockets;

        // Remote (client) port numbers.
        mt_const Uint32 rtp_port;
        mt_const Uint32 rtcp_port;

        // Destination UDP addresses. Unused, kept here for debugging.
        mt_const IpAddress rtp_addr;
        mt_const IpAddress rtcp_addr;

        // Note that using ImmediateConnectionSender would give better real-time
        // distribution (less bursts) at the expense of doing everything in
        // the source thread.
        //
        // Uninitialized when tcp_interleaved is true.
        Embed<DeferredConnectionSender> rtp_sender;
        Embed<DeferredConnectionSender> rtcp_sender;

        mt_sync_domain (stream) Uint16 sequence_number;

        mt_sync_domain (stream) bool got_keyframe;

        mt_mutex (mutex) Time first_sender_report_time_microsec;
        mt_mutex (mutex) bool first_sender_report_sent;

        // TODO Having to lock/unlock mutex for these counters is unfortunate
        mt_mutex (mutex) Uint32 packet_count;
        mt_mutex (mutex) Uint32 octet_count;

        mt_mutex (mutex) ReceiverMapping *receiver_mapping;

        RtpDataSession (EmbedContainer *embed_container);
    };

    struct BoundSessions;
    class RtspConnection;
    class RtspSessionCleanupList_name;

    class RtspSession : public Object,
                        public IntrusiveListElement< RtspSessionCleanupList_name >
    {
      public:
        mt_const WeakRef<RtspService>    weak_rtsp_service;
        mt_const WeakRef<RtspConnection> weak_rtsp_conn;

        mt_const StRef<String> session_id;

        mt_const Ref<BoundSessions> bound_sessions;

        mt_mutex (mutex) bool valid;
        mt_mutex (mutex) Time last_request_time_millisec;
        mt_mutex (mutex) bool playing;

        mt_mutex (mutex) Ref<MediaSource> media_source;
        mt_mutex (mutex) GenericInformer::SubscriptionKey stream_sbn;

        mt_mutex (mutex) Timers::TimerKey session_keepalive_timer;
        mt_mutex (mutex) List< Ref<RtspSession> >::Element *session_list_el;

        // It's not allowed to set 'audio_data_session' and 'video_data_session'
        // after subscribing to 'stream's events (i.e. after setting 'playing' to true).
        // Stream event handler assumes that 'audio_data_session' and 'video_data_session'
        // are mt_const.
        mt_mutex (mutex) Ref<RtpDataSession> audio_data_session;
        mt_mutex (mutex) Ref<RtpDataSession> video_data_session;

        mt_mutex (mutex) Ref<Object> cm_session_guard;

        mt_sync_domain ((stream, vod_source)) bool   got_timestamp_offset;
        mt_sync_domain ((stream, vod_source)) Uint64 timestamp_offset;
        mt_sync_domain ((stream, vod_source)) Uint64 last_timestamp;

        RtspSession (EmbedContainer *embed_container);
    };

    struct BoundSessions : public Object
    {
        // TODO synchronization - ?
        WeakRef<RtspSession> weak_audio_rtsp_session;
        WeakRef<RtspSession> weak_video_rtsp_session;

        mt_mutex (mutex) bool subscribed;

        mt_sync_domain (vod_source_frontend) Uint64 play_start_time_microsec;
        mt_sync_domain (vod_source_frontend) Uint64 first_dts_nanosec;
        mt_sync_domain (vod_source_frontend) bool   got_first_dts;

        BoundSessions (EmbedContainer * const embed_container)
            : Object                   (embed_container),
              subscribed               (false),
              play_start_time_microsec (0),
              first_dts_nanosec        (0),
              got_first_dts            (false)
        {}
    };

    typedef IntrusiveList< RtspSession, RtspSessionCleanupList_name > RtspSessionCleanupList;

    class RtspConnectionCleanupList_name;

    class RtspConnection : public Object,
                           public IntrusiveListElement< RtspConnectionCleanupList_name >
    {
      public:
        mt_const WeakRef<RtspService> weak_rtsp_service;
        mt_const IpAddress client_addr;
        mt_const IpAddress local_addr;

        mt_const Ref<TcpConnectionBlock> tcp_block;
        mt_const Ref<BoundSessions> bound_sessions;

        mt_mutex (mutex) bool valid;
        mt_mutex (mutex) Time last_request_time_millisec;

        mt_mutex (mutex) ReceiverMapping *receiver_mapping;

        mt_mutex (mutex) Uint64 start_time_millisec;
        mt_mutex (mutex) bool   got_start_time;
        mt_mutex (mutex) Uint64 duration_millisec;
        mt_mutex (mutex) bool   got_duration;

        mt_mutex (mutex) StRef<String>    stream_name; // mostly unused
        mt_mutex (mutex) Ref<MediaSource> media_source;
        mt_mutex (mutex) Ref<Object>      cm_session_guard;

        mt_mutex (mutex) Uint32 audio_rate;

        mt_sync_domain (http_frontend) StRef<String> audio_rtsp_session_id;
        mt_sync_domain (http_frontend) StRef<String> video_rtsp_session_id;

        mt_sync_domain (http_frontend) DataState data_state;
        mt_sync_domain (http_frontend) Size msg_len;
        mt_sync_domain (http_frontend) Size got_len;

        mt_mutex (mutex) PollGroup::PollableKey pollable_key;
        mt_mutex (mutex) Timers::TimerKey conn_keepalive_timer;
        mt_mutex (mutex) List< Ref<RtspConnection> >::Element *conn_list_el;

        RtspConnection (EmbedContainer *embed_container);
    };

    typedef IntrusiveList< RtspConnection, RtspConnectionCleanupList_name > RtspConnectionCleanupList;

    struct ReceiverMapping : public HashEntry<>
    {
        mt_const String          receiver_str;
        mt_const RtpDataSession *data_session;
        mt_const RtspSession    *session;
        mt_const RtspConnection *rtsp_conn;
    };

    typedef Hash< ReceiverMapping,
                  ConstMemory,
                  MemberExtractor< ReceiverMapping,
                                   String,
                                   &ReceiverMapping::receiver_str,
                                   ConstMemory,
                                   AccessorExtractor< String,
                                                      Memory,
                                                      &String::mem > >,
                  MemoryComparator<> >
            ReceiverMappingHash;

    mt_const Ref<MomentServer>        moment;
    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const Ref<PagePool>            page_pool;

    mt_const bool enable_tcp;
    mt_const bool tcp_only;
    mt_const bool udp_silence;

    mt_const Time session_request_timeout_millisec;
    mt_const Time conn_request_timeout_millisec;

    DeferredProcessor::Registration deferred_reg;

    Embed<TcpServer> tcp_server;
    mt_const Ref<RtpServerSockets> rtp_server_sockets;

    mt_const Uint16 server_rtp_port;
    mt_const Uint16 server_rtcp_port;

    mt_mutex (mutex) PollGroup::PollableKey srv_pollable_key;
    mt_mutex (mutex) PollGroup::PollableKey rtp_socket_pollable_key;
    mt_mutex (mutex) PollGroup::PollableKey rtcp_socket_pollable_key;

    mt_mutex (mutex) List< Ref<RtspConnection> > conn_list;
    mt_mutex (mutex) List< Ref<RtspSession> >    session_list;

    mt_mutex (mutex) RtspConnectionCleanupList rtsp_conn_cleanup_list;
    mt_mutex (mutex) RtspSessionCleanupList    rtsp_session_cleanup_list;

    mt_mutex (mutex) ReceiverMappingHash receiver_mapping_hash;

    mt_mutex (mutex) void destroyRtspSession (RtspSession * mt_nonnull session);

    mt_mutex (mutex) void destroyRtspSession_handleMediaStream (RtspSession * mt_nonnull session,
                                                                MediaStream * mt_nonnull stream);

    mt_mutex (mutex) void destroyRtspSession_handleVodSource (RtspSession * mt_nonnull session,
                                                              VodSource   * mt_nonnull vod_source);

    mt_mutex (mutex) void destroyRtspConnection (RtspConnection * mt_nonnull rtsp_conn);

    static void senderReportTick (void *_session);
    static void timeoutCheckTick (void *_self);

    void sendRtcpSenderReport (RtpDataSession * mt_nonnull data_session);

    mt_sync_domain (stream) void sendRtpVideoMessage (RtspSession    * mt_nonnull session,
                                                      ConstMemory     extra_header,
                                                      VideoMessage   * mt_nonnull video_msg,
                                                      PagePool       * mt_nonnull normalized_page_pool,
                                                      PagePool::Page *page,
                                                      Size            offs,
                                                      Size            data_len,
                                                      bool            last_nal);

    static void stream_doAudioMessage (AudioMessage * mt_nonnull audio_msg,
                                       RtspSession  * mt_nonnull session);

    static void stream_doVideoMessage (VideoMessage * mt_nonnull video_msg,
                                       RtspSession  * mt_nonnull session);

    static void stream_doClosed (RtspSession * mt_nonnull session);

    mt_iface (VideoStream::Events)
      static VideoStream::Events const stream_events;

      static void stream_audioMessage (AudioMessage * mt_nonnull audio_msg,
                                       void         *_bound_sessions);

      static void stream_videoMessage (VideoMessage * mt_nonnull video_msg,
                                       void         *_bound_sessions);

      static void stream_closed (void *_bound_sessions);
    mt_iface_end

    struct RateLimitTimerData;
    struct RateLimitTimerData_Audio;
    struct RateLimitTimerData_Video;

    static void rateLimitTimerTick (void *_data);

    mt_sync_domain (vod_source_frontend) static bool setRateLimitTimerIfNeeded (BoundSessions * mt_nonnull bound_sessions,
                                                                                RtspSession   * mt_nonnull session,
                                                                                MediaMessage  * mt_nonnull msg);

    mt_iface (VodSource::Frontend)
      static VodSource::Frontend const vod_source_frontend;

      mt_sync_domain (vod_source_frontend) static void vod_mediaMessage (MediaMessage * mt_nonnull msg,
                                                                         void         *_bound_sessions);

      mt_sync_domain (vod_source_frontend) static void vod_endOfSource (VodSource::EndReason  end_reason,
                                                                        void                 *_bound_sessions);
    mt_iface_end

    void sendNotFound             (RtspConnection * mt_nonnull rtsp_conn,
                                   Uint32          cseq);
    void sendBadRequest           (RtspConnection * mt_nonnull rtsp_conn,
                                   Uint32          cseq);
    void sendSessionNotFound      (RtspConnection * mt_nonnull rtsp_conn,
                                   Uint32          cseq);
    void sendUnsupportedTransport (RtspConnection * mt_nonnull rtsp_conn,
                                   Uint32          cseq);
    void sendNotImplemented       (RtspConnection * mt_nonnull rtsp_conn,
                                   Uint32          cseq);


    // _______________________________ DESCRIBE ________________________________

    struct RtspRequest_Describe_Data;

    void rtspRequest_Describe (HttpRequest    * mt_nonnull req,
                               RtspConnection * mt_nonnull rtsp_conn,
                               Uint32          cseq,
                               bool           * mt_nonnull ret_block_input);

    static void rtspRequest_Describe_startWatchingCallback (MediaSource                  *media_source,
                                                            ClientManager::ClientSession * mt_nonnull cm_session,
                                                            void                         *_data);

    bool rtspRequest_Describe_completeStartWatching (RtspRequest_Describe_Data    * mt_nonnull data,
                                                     RtspConnection               * mt_nonnull rtsp_conn,
                                                     MediaSource                  *media_source,
                                                     ClientManager::ClientSession * mt_nonnull cm_session);

    bool rtspRequest_Describe_stream_completeStartWatching (RtspRequest_Describe_Data    * mt_nonnull data,
                                                            RtspConnection               * mt_nonnull rtsp_conn,
                                                            MediaStream                  * mt_nonnull stream,
                                                            ClientManager::ClientSession * mt_nonnull cm_session);

    bool rtspRequest_Describe_vod_completeStartWatching (RtspRequest_Describe_Data    * mt_nonnull data,
                                                         RtspConnection               * mt_nonnull rtsp_conn,
                                                         VodSource                    * mt_nonnull vod_source,
                                                         ClientManager::ClientSession * mt_nonnull cm_session);

    static void rtspRequest_Describe_vod_querySourceInfoCallback (VodSourceInfo * mt_nonnull source_info,
                                                                  void          *_data);

    bool rtspRequest_Describe_vod_completeQuerySourceInfo (RtspRequest_Describe_Data    * mt_nonnull data,
                                                           RtspConnection               * mt_nonnull rtsp_conn,
                                                           ClientManager::ClientSession * mt_nonnull cm_session,
                                                           VodSourceInfo                * mt_nonnull source_info);

    void rtspRequest_Describe_finishOK (RtspRequest_Describe_Data    * mt_nonnull data,
                                        RtspConnection               * mt_nonnull rtsp_conn,
                                        ConstMemory                   sdp_mem,
                                        Uint32                        audio_rate);

    void rtspRequest_Describe_finishNotFound (RtspRequest_Describe_Data    * mt_nonnull data,
                                              RtspConnection               * mt_nonnull rtsp_conn,
                                              ClientManager::ClientSession * mt_nonnull cm_session,
                                              ConstMemory                   err_str);

    // _________________________________________________________________________


    void rtspRequest_Setup (HttpRequest    * mt_nonnull req,
                            RtspConnection * mt_nonnull rtsp_conn,
                            RtspSession    * mt_nonnull session,
                            Uint32          cseq,
                            Time            cur_time_millisec);


    // _________________________________ PLAY __________________________________

    void rtspRequest_Play (HttpRequest    * mt_nonnull req,
                           RtspConnection * mt_nonnull rtsp_conn,
                           RtspSession    * mt_nonnull session,
                           Uint32          cseq);

    mt_unlocks (mutex) Result rtspRequest_Play_handleMediaStream (HttpRequest    * mt_nonnull req,
                                                                  RtspConnection * mt_nonnull rtsp_conn,
                                                                  RtspSession    * mt_nonnull session,
                                                                  Uint32          cseq,
                                                                  MediaStream    * mt_nonnull _stream,
                                                                  bool            disable_udp_data);

    mt_unlocks (mutex) Result rtspRequest_Play_handleVodSource (HttpRequest    * mt_nonnull req,
                                                                RtspConnection * mt_nonnull rtsp_conn,
                                                                RtspSession    * mt_nonnull session,
                                                                Uint32          cseq,
                                                                VodSource      * mt_nonnull _vod_source,
                                                                bool            disable_udp_data);

    // _________________________________________________________________________


    mt_iface (HttpServer::Frontend)
      static HttpServer::Frontend const http_frontend;

      static void rtspRawData (Memory  mem,
                               Size   * mt_nonnull ret_accepted,
                               bool   * mt_nonnull ret_req_next,
                               bool   * mt_nonnull ret_block_input,
                               void   *_rtsp_conn);

      static void rtspRequest (HttpRequest * mt_nonnull req,
                               bool        * mt_nonnull ret_block_input,
                               void        *_rtsp_conn);

      static void rtspClosed (HttpRequest *req,
                              Exception   *exc_,
                              void        *_rtsp_conn);
    mt_iface_end

    bool acceptOneConnection ();

    mt_iface (TcpServer::Frontend)
      static TcpServer::Frontend const tcp_server_frontend;

      static void accepted (void *_self);
    mt_iface_end

  public:
    mt_throws Result bind (IpAddress addr);

    Result start ();

    mt_const Result init (MomentServer * mt_nonnull moment,
                          PagePool     * mt_nonnull page_pool,
                          bool          enable_tcp,
                          bool          tcp_only,
                          bool          udp_silence,
                          Time          timeout_check_interval_millisec,
                          Time          session_request_timeout_millisec,
                          Time          conn_request_timeout_millisec);

     RtspService (EmbedContainer *embed_container);
    ~RtspService ();
};

}


#endif /* MOMENT_RTSP__RTSP_SERVICE__H__ */

