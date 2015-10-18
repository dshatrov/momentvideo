/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_RTSP__RTSP_CLIENT__H__
#define MOMENT_RTSP__RTSP_CLIENT__H__


#include <moment-rtsp/rtp_port_pair.h>
#include <moment/stream.h>
#include <moment/h264_parser.h>
#include <moment/fetch_connection.h>


namespace MomentRtsp {

using namespace Moment;

extern StatCounter_AtomicInt stat_counter__rtsp_client;

struct RtspPeerDesc
{
    IpAddress   server_addr;
    ConstMemory uri;
    ConstMemory login;
    ConstMemory password;
};

struct RtspClientConfig
{
    bool tcp_interleaved;
    bool enable_audio;
    bool enable_video;
    Time no_avdata_check_interval_millisec;
    Time no_avdata_timeout_millisec;
    bool drop_sei_nal_units;

    RtspClientConfig ()
        : tcp_interleaved (false),
          enable_audio (true),
          enable_video (true),
          no_avdata_check_interval_millisec (1000),
          no_avdata_timeout_millisec (10000),
          drop_sei_nal_units (false)
    {}
};

class RtspClient : public FetchConnection
{
  private:
    StateMutex mutex;

    StatCounter_AtomicInt::Unit<&stat_counter__rtsp_client> stat_unit;

    enum_beg (State)
        enum_values (
                Initial,
                OptionsSent,
                DescribeSent,
                SetupAudioSent,
                SetupVideoSent,
                PlaySent,
                InterleavedData)
    enum_end (State)

    enum_beg (DataState)
        enum_values (Header, Data)
    enum_end (DataState)

    static RtpPortPair audio_port_pair;
    static RtpPortPair video_port_pair;

    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const Ref<PagePool> page_pool;
    mt_const IpAddress server_addr;
    mt_const StRef<String> login;
    mt_const StRef<String> password;
    mt_const bool tcp_interleaved;

    mt_const Ref<MediaStream> stream;

    mt_const bool drop_sei_nal_units;

    mt_const Time reordering_delay_microsec = 250000;

    mt_const Time no_avdata_timeout_millisec;
    mt_mutex (mutex) Time last_frame_time_millisec;
    mt_mutex (mutex) bool got_first_frame;

    mt_const Cb<FetchConnection::FetchConnectionFrontend> fetch_connection_frontend;


    struct RtpPacket : public IntrusiveListElement<>
    {
        PagePool::PageListHead page_list;
        Size   packet_len;
        Size   packet_len_without_padding;
        Uint16 rtp_seq_num;
        Time   arrival_time_microsec;
        bool   marker;
        Uint16 sequence_number;
        Time   timestamp;
    };
    typedef IntrusiveList< RtpPacket, IntrusiveList_name, DeleteAction<RtpPacket> > RtpPacketList;

    struct RtpConnection
    {
        mt_const RtspClient *rtsp_client;
        mt_const bool is_audio;
        mt_const bool enable;

        mt_const Uint16 rtp_port;
        mt_const Uint16 rtcp_port;

        Embed<UdpSocket> rtp_socket;
        Embed<UdpSocket> rtcp_socket;

        Embed<DeferredConnectionSender> rtp_sender;
        Embed<DeferredConnectionSender> rtcp_sender;

        Embed<ConnectionReceiver> rtp_receiver;
        Embed<ConnectionReceiver> rtcp_receiver;

        // If 'available' is 'true', then we have all necessary information
        // about this AVC/AAC stream and should accept incoming RTP packets.
        mt_mutex (mutex) bool available;

        mt_mutex (mutex) StRef<String> control_url;

        mt_mutex (mutex) PollGroup::PollableKey rtp_pollable_key;
        mt_mutex (mutex) PollGroup::PollableKey rtcp_pollable_key;
 
        mt_mutex (mutex) bool   got_expected_seq;
        mt_mutex (mutex) Uint64 expected_seq;
        mt_mutex (mutex) bool   got_expected_rtptime;
        mt_mutex (mutex) Uint64 expected_rtptime;

        mt_mutex (mutex) Uint64 initial_rtptime;

        mt_mutex (mutex) RtpPacketList reordering_queue;

        RtpConnection (RtspClient * const mt_nonnull rtsp_client,
                       bool         const is_audio)
            : rtsp_client          (rtsp_client),
              is_audio             (is_audio),
              enable               (true),
              rtp_port             (0),
              rtcp_port            (0),
              rtp_socket           (rtsp_client /* embed_container */),
              rtcp_socket          (rtsp_client /* embed_container */),
              rtp_sender           (rtsp_client /* embed_container */, true /* datagram_mode */),
              rtcp_sender          (rtsp_client /* embed_container */, true /* datagram_mode */),
              rtp_receiver         (rtsp_client /* embed_container */),
              rtcp_receiver        (rtsp_client /* embed_container */),
              available            (false),
              got_expected_seq     (false),
              expected_seq         (0),
              got_expected_rtptime (false),
              expected_rtptime     (0),
              initial_rtptime      (0)
        {}
    };

    RtpConnection rtp_audio;
    RtpConnection rtp_video;

    Embed<TcpConnection>            tcp_conn;
    Embed<DeferredConnectionSender> conn_sender;
    Embed<ConnectionReceiver>       conn_receiver;
    Embed<HttpServer>               http_server;

    mt_mutex (mutex) bool closed;
    mt_mutex (mutex) bool close_pending;
    mt_mutex (mutex) DisconnectReason close_pending__disconnect_reason;

    mt_mutex (mutex) PreassemblyBuffer preassembly_buffer;

    mt_mutex (mutex) PollGroup::PollableKey pollable_key;

    mt_mutex (mutex) Uint32 audio_payload_type;
    mt_mutex (mutex) Uint32 video_payload_type;

    mt_mutex (mutex) Uint32 audio_clock_rate;
    mt_mutex (mutex) Uint32 audio_channels;

    mt_mutex (mutex) Uint32 rtsp_cseq;
    mt_mutex (mutex) StRef<String> session_id;

    /* Unused
    mt_mutex (mutex) Uint32 ssrc_audio;
    mt_mutex (mutex) Uint32 ssrc_video;
    */

    mt_sync_domain (http_server_frontend) StRef<String> target_uri;

    mt_sync_domain (http_server_frontend) State state;
    mt_sync_domain (http_server_frontend) bool got_describe_reply;
    mt_sync_domain (http_server_frontend) DataState data_state;
    mt_sync_domain (http_server_frontend) Byte channel_id;
    mt_sync_domain (http_server_frontend) Size msg_len;
    mt_sync_domain (http_server_frontend) Size got_len;

    mt_sync_domain (http_server_frontend) bool          auth_required;
    mt_sync_domain (http_server_frontend) bool          auth_digest;
    mt_sync_domain (http_server_frontend) StRef<String> auth_header;
    mt_sync_domain (http_server_frontend) StRef<String> auth_realm;
    mt_sync_domain (http_server_frontend) StRef<String> auth_nonce;
    mt_sync_domain (http_server_frontend) StRef<String> auth_opaque;
    mt_sync_domain (http_server_frontend) ConstMemory   last_method;
    mt_sync_domain (http_server_frontend) StRef<String> last_uri;

    // TODO Come up with a way to use sync domain for page_list.
    mt_mutex (mutex) PagePool::PageListHead page_list;

    mt_mutex (mutex) PagePool::PageListHead audio_fragmented_list;
    mt_mutex (mutex) Size audio_fragmented_len;

    mt_mutex (mutex) PagePool::PageListHead video_fragmented_list;
    mt_mutex (mutex) Size   video_fragmented_len;
    mt_mutex (mutex) bool   discard_video_fragment;
    mt_mutex (mutex) Uint16 expected_video_fragment_seq_num;

    mt_mutex (mutex) H264Parser h264_parser;

    mt_mutex (mutex) Timers::TimerKey ping_timer;

    mt_mutex (mutex) void rtpConnectionStart (RtpConnection * mt_nonnull rtp_conn);
    mt_mutex (mutex) void doStart ();

    mt_mutex (mutex) void rtpConnectionRelease (RtpConnection * mt_nonnull rtp_conn);
    mt_unlocks (mutex) void destroySelf (DisconnectReason disconnect_reason);

    mt_mutex (mutex) void processRtpPacket (RtpConnection * mt_nonnull rtp_conn);

    mt_mutex (mutex) void processRtpPacket_Audio (PagePool::PageListHead const &packet_pages,
                                                  Size   packet_len_without_padding,
                                                  bool   marker,
                                                  Uint64 timestamp);

    mt_mutex (mutex) void processRtpPacket_Video (PagePool::PageListHead const &packet_pages,
                                                  Size   packet_len_without_padding,
                                                  bool   marker,
                                                  Uint16 sequence_number,
                                                  Uint64 timestamp);

    mt_mutex (mutex) void processNal (Byte nal_type,
                                      PagePool::PageListHead const * mt_nonnull nal_pages,
                                      Size nal_len,
                                      Size nal_offs,
                                      Time timestamp,
                                      bool marker);

    mt_iface (H264Parser::Callbacks)
      static H264Parser::Callbacks const h264_callbacks;

      mt_mutex (mutex) static void h264_parameterSets (PagePool::PageListHead *page_list,
                                                       Size                    msg_offs,
                                                       Size                    msg_len,
                                                       Time                    pts,
                                                       Time                    dts,
                                                       void                   *_self);

      mt_mutex (mutex) static void h264_accessUnit (PagePool::PageListHead *page_list,
                                                    Size                    msg_offs,
                                                    Size                    msg_len,
                                                    bool                    is_keyframe,
                                                    Time                    pts,
                                                    Time                    dts,
                                                    void                   *_self);
    mt_iface_end

    mt_mutex (mutex) void nextStateAction ();
    mt_mutex (mutex) bool doNextStateAction ();
    mt_mutex (mutex) void repeatLastStateAction ();

    mt_mutex (mutex) bool stateAction_sendOptions ();
    mt_mutex (mutex) bool stateAction_sendDescribe ();

    mt_mutex (mutex) bool stateAction_doSendSetup    (RtpConnection * mt_nonnull rtp_conn);
    mt_mutex (mutex) bool stateAction_sendSetupAudio ();
    mt_mutex (mutex) bool stateAction_sendSetupVideo ();

    mt_mutex (mutex) bool stateAction_sendPlay ();

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const audio_rtp_receiver_frontend;

      static Receiver::ProcessInputResult audioRtpInput (Memory  mem,
                                                         Size   *ret_accepted,
                                                         void   *_self);

      static void audioRtpEof (Memory  unprocessed_mem,
                               void   *_self);

      static void audioRtpError (Exception *exc_,
                                 Memory     unprocessed_mem,
                                 void      *_self);
    mt_iface_end

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const audio_rtcp_receiver_frontend;

      static Receiver::ProcessInputResult audioRtcpInput (Memory  mem,
                                                          Size   *ret_accepted,
                                                          void   *_self);

      static void audioRtcpEof (Memory  unprocessed_mem,
                                void   *_self);

      static void audioRtcpError (Exception *exc_,
                                  Memory     unprocessed_mem,
                                  void      *_self);
    mt_iface_end

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const video_rtp_receiver_frontend;

      static Receiver::ProcessInputResult videoRtpInput (Memory  mem,
                                                         Size   *ret_accepted,
                                                         void   *_self);

      static void videoRtpEof (Memory  unprocessed_mem,
                               void   *_self);

      static void videoRtpError (Exception *exc_,
                                 Memory     unprocessed_mem,
                                 void      *_self);
    mt_iface_end

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const video_rtcp_receiver_frontend;

      static Receiver::ProcessInputResult videoRtcpInput (Memory  mem,
                                                          Size   *ret_accepted,
                                                          void   *_self);

      static void videoRtcpEof (Memory  unprocessed_mem,
                                void   *_self);

      static void videoRtcpError (Exception *exc_,
                                  Memory     unprocessed_mem,
                                  void      *_self);
    mt_iface_end

    mt_iface (TcpConnection::Frontend)
      static TcpConnection::Frontend const tcp_conn_frontend;

      static void connected (Exception *exc_,
                             void      *_self);
    mt_iface_end

    void sendUdpNatPinholePackets (HttpRequest * mt_nonnull req);

    mt_sync_domain (http_server_frontend) StRef<String> getAuthHeader ();
    mt_sync_domain (http_server_frontend) StRef<String> makeDigestAuthHeader ();

    mt_sync_domain (http_server_frontend) void rtspRequest_401_Unauthorized (HttpRequest * mt_nonnull req);

    mt_unlocks (mutex) void rtspRequest_OptionsReply  ();
    mt_unlocks (mutex) void rtspRequest_DescribeReply (HttpRequest * mt_nonnull req,
                                                       Uint32       status_code);
    mt_unlocks (mutex) void rtspRequest_SetupReply    (HttpRequest * mt_nonnull req);
    mt_unlocks (mutex) void rtspRequest_PlayReply     (HttpRequest * mt_nonnull req);

    void rtspRequest_Request (HttpRequest * mt_nonnull req);

    mt_iface (HttpServer::Frontend)
      static HttpServer::Frontend const http_server_frontend;

      static void rtspRawData (Memory  mem,
                               Size   * mt_nonnull ret_accepted,
                               bool   * mt_nonnull ret_req_next,
                               bool   * mt_nonnull ret_block_input,
                               void   *_self);

      static void rtspRequest (HttpRequest * mt_nonnull req,
                               bool        * mt_nonnull ret_block_input,
                               void        *_self);

      static void rtspMessageBody (HttpRequest * mt_nonnull req,
                                   Memory       mem,
                                   bool         end_of_request,
                                   Size        * mt_nonnull ret_accepted,
                                   bool        * mt_nonnull ret_block_input,
                                   void        *_self);

      static void rtspClosed (HttpRequest *req,
                              Exception   *exc_,
                              void        *_self);
    mt_iface_end

    mt_iface (PreassemblyBuffer::Frontend)
      static PreassemblyBuffer::Frontend const preassembly_frontend;

      mt_mutex (mutex) static void preassemblyData (ConstMemory  mem,
                                                    bool         limit_overflow,
                                                    void        *_self);
    mt_iface_end

    mt_mutex (mutex) Result processSdpParameterSets (ConstMemory parameter_sets,
                                                     ConstMemory profile_level_id);

    mt_mutex (mutex) Result processAacConfig (ConstMemory aac_config);

    mt_mutex (mutex) void sendGetParameter ();

    static void pingTimerTick (void *_self);

    static void noVideoTimerTick (void *_self);

    mt_const void rtpConnectionInit (RtpConnection       * mt_nonnull rtp_conn,
                                     ServerThreadContext * mt_nonnull thread_ctx,
                                     bool                 enable);

    mt_mutex (mutex) void releaseRtpConnection (RtpConnection * mt_nonnull rtp_conn);

  public:
    Result start ();

    mt_const Result init (ServerThreadContext * mt_nonnull thread_ctx,
                          PagePool            * mt_nonnull page_pool,
                          ConstMemory          uri,
                          RtspClientConfig    * mt_nonnull client_config,
                          MediaStream         * mt_nonnull stream,
                          CbDesc<FetchConnection::FetchConnectionFrontend> const &fetch_connection_frontend);

    mt_const Result init_rtspPeerDesc (ServerThreadContext * mt_nonnull thread_ctx,
                                       PagePool            * mt_nonnull page_pool,
                                       RtspPeerDesc        * mt_nonnull peer_desc,
                                       RtspClientConfig    * mt_nonnull client_config,
                                       MediaStream         * mt_nonnull stream,
                                       CbDesc<FetchConnection::FetchConnectionFrontend> const &fetch_connection_frontend);

     RtspClient (EmbedContainer *embed_container);
    ~RtspClient ();
};

}


#endif /* MOMENT_RTSP__RTSP_CLIENT__H__ */

