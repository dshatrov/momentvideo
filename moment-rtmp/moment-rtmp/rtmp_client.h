/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__RTMP_CLIENT__H__
#define MOMENT__RTMP_CLIENT__H__


#include <libmary/libmary.h>

#include <moment-rtmp/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpClient : public FetchConnection
{
  private:
    StateMutex mutex;

    enum ConnectionState {
        ConnectionState_Initial,
        ConnectionState_Connect,
        ConnectionState_ConnectSent,
        ConnectionState_CreateStreamSent,
        ConnectionState_PlaySent,
        ConnectionState_Streaming
    };

    mt_const Ref<ServerThreadContext> thread_ctx;

    Embed<RtmpConnection>           rtmp_conn;
    Embed<TcpConnection>            tcp_conn;
    Embed<DeferredConnectionSender> conn_sender;
    Embed<ConnectionReceiver>       conn_receiver;

    mt_const Ref<Stream> stream;

    mt_const IpAddress     server_addr;
    mt_const StRef<String> app_name;
    mt_const StRef<String> stream_name;
    mt_const bool          momentrtmp_proto;

    mt_const Time no_video_timeout_millisec;
    mt_mutex (stream) Time last_frame_time_millisec;
    mt_mutex (stream) bool got_first_frame;
    mt_mutex (stream) bool started;

    void streamLock ()   { if (stream) stream->lock ();   else mutex.lock ();   }
    void streamUnlock () { if (stream) stream->unlock (); else mutex.unlock (); }

    mt_const Cb<FetchConnection::FetchConnectionFrontend> fetch_connection_frontend;

    mt_mutex (mutex) PollGroup::PollableKey pollable_key;

    mt_sync_domain (rtmp_conn_frontend) ConnectionState conn_state;

    mt_iface (TcpConnection::Frontend)
      static TcpConnection::Frontend const tcp_conn_frontend;

      static void connected (Exception *exc_,
                             void      *_rtmp_conn);
    mt_iface_end

    mt_iface (RtmpConnection::Backend)
      static RtmpConnection::Backend const rtmp_conn_backend;

      static void closeRtmpConn (DisconnectReason  disconnect_reason,
                                 void             *cb_data);
    mt_iface_end

    mt_iface (RtmpConnection::Frontend)
      static RtmpConnection::Frontend const rtmp_conn_frontend;

      static mt_sync_domain (rtmp_conn_frontend)
              Result handshakeComplete (void *cb_data);

      static mt_sync_domain (rtmp_conn_frontend)
              Result commandMessage (bool                            is_data,
                                     MediaMessage                   * mt_nonnull msg,
                                     Uint32                          msg_stream_id,
                                     AmfEncoding                     amf_encoding,
                                     RtmpConnection::ConnectionInfo * mt_nonnull conn_info,
                                     void                           *_self);

      static mt_sync_domain (rtmp_conn_frontend)
              Result audioMessage (AudioMessage * mt_nonnull audio_msg,
                                   void         *_self);

      static mt_sync_domain (rtmp_conn_frontend)
              Result videoMessage (VideoMessage * mt_nonnull video_msg,
                                   void         *_self);

      static void closed (Exception *exc_,
                          void      *_self);
    mt_iface_end

    static void noVideoTimerTick (void *_self);

  public:
    mt_iface (FetchConnection)
      // Should be called only once.
      Result start ();
    mt_iface_end

    struct RtmpClientConfig
    {
        bool momentrtmp_proto                 = false;
        Time ping_timeout_millisec            = 0;
        Time send_delay_millisec              = 0;
        Time no_video_check_interval_millisec = 0;
        Time no_video_timeout_millisec        = 0;

        RtmpClientConfig (bool const momentrtmp_proto,
                          bool const ping_timeout_millisec,
                          Time const send_delay_millisec,
                          Time const no_video_check_interval_millisec,
                          Time const no_video_timeout_millisec)
            : momentrtmp_proto                 (momentrtmp_proto),
              ping_timeout_millisec            (ping_timeout_millisec),
              send_delay_millisec              (send_delay_millisec),
              no_video_check_interval_millisec (no_video_check_interval_millisec),
              no_video_timeout_millisec        (no_video_timeout_millisec)
        {}

        RtmpClientConfig () {}
    };

    mt_const void init (ServerThreadContext    * mt_nonnull thread_ctx,
                        PagePool               * mt_nonnull page_pool,
                        MediaStream            *stream,
                        IpAddress               server_addr,
                        ConstMemory             app_name,
                        ConstMemory             stream_name,
                        RtmpClientConfig const &client_config,
                        CbDesc<FetchConnection::FetchConnectionFrontend> const &fetch_connection_frontend);

     RtmpClient (EmbedContainer *embed_container);
    ~RtmpClient ();
};

}


#endif /* MOMENT__RTMP_CLIENT__H__ */

