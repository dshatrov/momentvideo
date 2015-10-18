/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__RTMP_PUSH_PROTOCOL__H__
#define MOMENT__RTMP_PUSH_PROTOCOL__H__


#include <libmary/libmary.h>
#include <moment/moment_server.h>
#include <moment/push_protocol.h>

#include <moment-rtmp/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpPushConnection : public PushConnection
{
  private:
    StateMutex mutex;

    enum ConnectionState {
        ConnectionState_Connect,
        ConnectionState_ConnectSent,
        ConnectionState_CreateStreamSent,
        ConnectionState_PublishSent,
        ConnectionState_Streaming
    };

    class Session : public Object
    {
      public:
        // Must be referenced externally to use.
        RtmpPushConnection *rtmp_push_conn;

        Embed<RtmpConnection>           rtmp_conn;
        Embed<TcpConnection>            tcp_conn;
        Embed<DeferredConnectionSender> conn_sender;
        Embed<ConnectionReceiver>       conn_receiver;

        // Synchronized by 'rtmp_conn_frontend'.
        ConnectionState conn_state;

        AtomicInt publishing;

        mt_mutex (mutex) PollGroup::PollableKey pollable_key;

         Session (EmbedContainer *embed_container);
        ~Session ();
    };

    mt_const ServerThreadContext *thread_ctx;
    mt_const Timers *timers;
    mt_const PagePool *page_pool;

    mt_const Ref<VideoStream> video_stream;

    mt_const IpAddress server_addr;
    mt_const StRef<String> username;
    mt_const StRef<String> password;
    mt_const StRef<String> app_name;
    mt_const StRef<String> stream_name;
    mt_const Time ping_timeout_millisec;
    mt_const bool momentrtmp_proto;

    mt_mutex (mutex) Ref<Session> cur_session;
    mt_mutex (mutex) Timers::TimerKey reconnect_timer;

    mt_mutex (mutex) bool metadata_sent;

    mt_mutex (mutex) void destroySession  (Session * mt_nonnull session);
    mt_mutex (mutex) void startNewSession (Session *old_session);

    mt_mutex (mutex) void setReconnectTimer ();
    mt_mutex (mutex) void deleteReconnectTimer ();

    static void reconnectTimerTick (void *_self);

    void scheduleReconnect (Session *old_session);

    mt_iface (TcpConnection::Frontend)
      static TcpConnection::Frontend const tcp_conn_frontend;

      static void connected (Exception *exc_,
                             void      *_session);
    mt_iface_end

    mt_iface (RtmpConnection::Backend)
      static RtmpConnection::Backend const rtmp_conn_backend;

      static void closeRtmpConn (DisconnectReason  disconnect_reason,
                                 void             *_session);
    mt_iface_end

    mt_iface (VideoStream::FrameSaver::FrameHandler)
      static FrameSaver::FrameHandler const saved_frame_handler;

      static Result savedAudioFrame (AudioMessage * mt_nonnull audio_msg,
                                     void         *_session);

      static Result savedVideoFrame (VideoMessage * mt_nonnull video_msg,
                                     void         *_session);
    mt_iface_end

    mt_iface (RtmpConnection::Frontend)
      static RtmpConnection::Frontend const rtmp_conn_frontend;

      static Result handshakeComplete (void *_session);

      static Result commandMessage (bool                            is_data,
                                    MediaMessage                   * mt_nonnull msg,
                                    Uint32                          msg_stream_id,
                                    AmfEncoding                     amf_encoding,
                                    RtmpConnection::ConnectionInfo * mt_nonnull conn_info,
                                    void                           *_session);

      static void closed (Exception *exc_,
                          void      *_session);
    mt_iface_end

    mt_iface (VideoStream::Events)
      static VideoStream::Events const stream_events;

      static void audioMessage (AudioMessage * mt_nonnull msg,
                                void         *_self);

      static void videoMessage (VideoMessage * mt_nonnull msg,
                                void         *_self);

      static void mediaDesc (MediaDesc_Audio *audio_desc,
                             bool             set_audio,
                             MediaDesc_Video *video_desc,
                             bool             set_video,
                             void            *_self);
    mt_iface_end

  public:
    mt_const void init (ServerThreadContext * mt_nonnull _thread_ctx,
                        PagePool            * mt_nonnull _page_pool,
                        VideoStream         *_video_stream,
                        IpAddress            _server_addr,
                        ConstMemory          _username,
                        ConstMemory          _password,
                        ConstMemory          _app_name,
                        ConstMemory          _stream_name,
                        Time                 _ping_timeout_millisec,
                        bool                 _momentrtmp_proto);

     RtmpPushConnection (EmbedContainer *embed_container);
    ~RtmpPushConnection ();
};

class RtmpPushProtocol : public PushProtocol
{
  private:
    mt_const Ref<MomentServer> moment;
    mt_const Time ping_timeout_millisec;

  public:
    mt_iface (PushProtocol)
      mt_throws Ref<PushConnection> connect (VideoStream * mt_nonnull video_stream,
                                             ConstMemory  uri,
                                             ConstMemory  username,
                                             ConstMemory  password);
    mt_iface_end

    mt_const void init (MomentServer * mt_nonnull moment,
                        Time          ping_timeout_millisec);

    RtmpPushProtocol (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__RTMP_PUSH_PROTOCOL__H__ */

