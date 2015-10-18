/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_moment.h>

#include <moment-rtmp/rtmp_push_protocol.h>


using namespace M;

namespace Moment {

RtmpPushConnection::Session::Session (EmbedContainer * const embed_container)
    : Object        (embed_container),
      rtmp_conn     (this /* embed_container */),
      tcp_conn      (this /* embed_container */),
      conn_sender   (this /* embed_container */),
      conn_receiver (this /* embed_container */),
      conn_state    (ConnectionState_Connect),
      publishing    (0),
      pollable_key  (NULL)
{
    logD_ (_this_func_);
}

RtmpPushConnection::Session::~Session ()
{
    logD_ (_this_func_);
}

mt_mutex (mutex) void
RtmpPushConnection::destroySession (Session * const mt_nonnull session)
{
    session->rtmp_conn->close_noBackendCb ();

    if (session->pollable_key) {
        thread_ctx->getPollGroup()->removePollable (session->pollable_key);
        session->pollable_key = NULL;
    }
}

mt_mutex (mutex) void
RtmpPushConnection::startNewSession (Session * const old_session)
{
    logD_ (_func_);

    if (old_session) {
        if (old_session != cur_session) {
            logD_ (_func, "session mismatch: 0x", fmt_hex, (UintPtr) old_session, ", 0x", (UintPtr) cur_session.ptr());
            return;
        }

        destroySession (old_session);
    }

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    Ref<Session> const session = grabNewObject <Session> ();
    cur_session = session;

    session->rtmp_push_conn = this;

    session->conn_sender->setConnection (session->tcp_conn);
    session->conn_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    // RtmpConnection sets sender frontend.

    session->conn_receiver->init (session->tcp_conn,
                                  thread_ctx->getDeferredProcessor());
    session->conn_receiver->setFrontend (session->rtmp_conn->getReceiverFrontend());

    session->rtmp_conn->init (NULL /* dump_stream */,
                              timers,
                              page_pool,
                              // It is expected that there will be few push RTMP conns.
                              // Using non-zero send delay gives negligible performance
                              // increase in this case.
                              0    /* send_delay_millisec */,
                              ping_timeout_millisec,
                              momentrtmp_proto);

    // 'session' is surely referenced when a callback is called, because it serves
    // as a coderef container for 'rtmp_conn'. Same for 'tcp_conn'.
    session->rtmp_conn->setBackend  (CbDesc<RtmpConnection::Backend>  (&rtmp_conn_backend,  session, this));
    session->rtmp_conn->setFrontend (CbDesc<RtmpConnection::Frontend> (&rtmp_conn_frontend, session, this));
    session->rtmp_conn->setSender (session->conn_sender);

    session->tcp_conn->setFrontend (
            CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, session, this));

    if (!session->tcp_conn->open ()) {
        logE_ (_func, "tcp_conn.open() failed: ", exc->toString());
        goto _failure;
    }

    if (!thread_ctx->getPollGroup()->addPollable_beforeConnect (session->tcp_conn->getPollable(),
                                                                &session->pollable_key))
    {
        logE_ (_func, "addPollable() failed: ", exc->toString());
        goto _failure;
    }

    {
        TcpConnection::ConnectResult const connect_res = session->tcp_conn->connect (server_addr);
        if (connect_res == TcpConnection::ConnectResult_Error) {
            logE_ (_func, "Could not connect to server: ", exc->toString());
            goto _failure;
        }

        if (!thread_ctx->getPollGroup()->addPollable_afterConnect (session->tcp_conn->getPollable(),
                                                                   &session->pollable_key))
        {
            logE_ (_func, "addPollable() failed: ", exc->toString());
            goto _failure;
        }

        if (connect_res == TcpConnection::ConnectResult_Connected) {
            session->rtmp_conn->startClient ();
            session->conn_receiver->start ();
        } else
            assert (connect_res == TcpConnection::ConnectResult_InProgress);
    }

    return;

_failure:
    destroySession (session);
    cur_session = NULL;

    setReconnectTimer ();
}

mt_mutex (mutex) void
RtmpPushConnection::setReconnectTimer ()
{
    logD_ (_func_);

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    reconnect_timer = timers->addTimer (CbDesc<Timers::TimerCallback> (reconnectTimerTick, this, this),
                                        // TODO Config parameter for the timeout.
                                        1     /* time_seconds */,
                                        false /* periodical */,
                                        false /* auto_delete */);
}

mt_mutex (mutex) void
RtmpPushConnection::deleteReconnectTimer ()
{
    logD_ (_func_);

    if (reconnect_timer) {
        timers->deleteTimer (reconnect_timer);
        reconnect_timer = NULL;
    }
}

void
RtmpPushConnection::reconnectTimerTick (void * const _self)
{
    logD_ (_func_);

    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    logD_ (_func, "calling deleteReconnectTimer()");
    self->deleteReconnectTimer ();

    if (self->cur_session) {
        self->mutex.unlock ();
        return;
    }

    self->startNewSession (NULL /* old_session */);
    self->mutex.unlock ();
}

void
RtmpPushConnection::scheduleReconnect (Session * const old_session)
{
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) old_session);

    mutex.lock ();
    if (old_session != cur_session) {
        logD_ (_func, "session mismatch: 0x", fmt_hex, (UintPtr) old_session, ", 0x", (UintPtr) cur_session.ptr());
        mutex.unlock ();
        return;
    }

    destroySession (old_session);
    cur_session = NULL;

    setReconnectTimer ();

    mutex.unlock ();
}

TcpConnection::Frontend const RtmpPushConnection::tcp_conn_frontend = {
    connected
};

void
RtmpPushConnection::connected (Exception * const exc_,
                               void      * const _session)
{
    logD_ (_func_);

    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    if (exc_) {
        logE_ (_func, "Could not connect to server: ", exc_->toString());
        self->scheduleReconnect (session);
        return;
    }

    session->rtmp_conn->startClient ();
    session->conn_receiver->start ();
}

RtmpConnection::Backend const RtmpPushConnection::rtmp_conn_backend = {
    closeRtmpConn
};

void
RtmpPushConnection::closeRtmpConn (DisconnectReason   const disconnect_reason,
                                   void             * const _session)
{
    logD_ (_func, "disconnect_reason ", disconnect_reason);

    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    self->scheduleReconnect (session);
}

RtmpConnection::Frontend const RtmpPushConnection::rtmp_conn_frontend = {
    handshakeComplete,
    commandMessage,
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    NULL /* senderStateChanged */,
    closed
};

Result
RtmpPushConnection::handshakeComplete (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    session->conn_state = ConnectionState_ConnectSent;
    session->rtmp_conn->sendConnect (self->app_name->mem(),
                                     "http://127.0.0.1/page_url",
                                     "http://127.0.0.1/swf_url",
                                     "rtmp://127.0.0.1/tc_url");

    return Result::Success;
}

FrameSaver::FrameHandler const RtmpPushConnection::saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame,
    NULL /* mediaDesc */
};

Result
RtmpPushConnection::savedAudioFrame (AudioMessage * const mt_nonnull audio_msg,
                                     void         * const _session)
{
    Session * const session = static_cast <Session*> (_session);

//    AudioMessage tmp_audio_msg = *audio_msg;
//    tmp_audio_msg.timestamp = 0;
//
//    session->rtmp_conn->sendAudioMessage (&tmp_audio_msg);

    session->rtmp_conn->sendAudioMessage (audio_msg);
    return Result::Success;
}

Result
RtmpPushConnection::savedVideoFrame (VideoMessage * const mt_nonnull video_msg,
                                     void         * const _session)
{
    Session * const session = static_cast <Session*> (_session);

//    VideoMessage tmp_video_msg = *video_msg;
//    tmp_video_msg.timestamp = 0;
//
//    session->rtmp_conn->sendVideoMessage (&tmp_video_msg);

    session->rtmp_conn->sendVideoMessage (video_msg);
    return Result::Success;
}

Result
RtmpPushConnection::commandMessage (bool                             const is_data,
                                    MediaMessage                   * const mt_nonnull msg,
                                    Uint32                           const /* msg_stream_id */,
                                    AmfEncoding                      const /* amf_encoding */,
                                    RtmpConnection::ConnectionInfo * const mt_nonnull /* conn_info */,
                                    void                           * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    RtmpPushConnection * const self = session->rtmp_push_conn;

    if (msg->msg_len == 0)
        return Result::Success;

    PagePool::PageListArray pl_array (msg->page_list.first, msg->msg_offset, msg->msg_len);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg->msg_len);

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
                               &method_name_len,
                               NULL /* ret_full_len */))
    {
        logE_ (_func, "Could not decode method name");
        return Result::Failure;
    }
    ConstMemory method (method_name, method_name_len);

    if (is_data) {
        logD_ (_self_func, "data message ignored: ", method);
        return Result::Success;
    }

    if (equal (method, "_result")) {
        switch (session->conn_state) {
            case ConnectionState_ConnectSent: {
                session->rtmp_conn->sendCreateStream ();
                session->conn_state = ConnectionState_CreateStreamSent;
            } break;
            case ConnectionState_CreateStreamSent: {
                double stream_id;
                if (!decoder.decodeNumber (&stream_id)) {
                    logE_ (_func, "Could not decode stream_id");
                    return Result::Failure;
                }

                session->rtmp_conn->sendPublish (self->stream_name->mem(), "live");

                session->conn_state = ConnectionState_Streaming;

                self->video_stream->lock ();
                self->video_stream->getFrameSaver()->reportSavedFrames (&saved_frame_handler, session);
                session->publishing.set (1);
                self->video_stream->unlock ();
            } break;
            case ConnectionState_PublishSent: {
              // Unused
            } break;
            default:
              // Ignoring
                ;
        }
    } else
    if (equal (method, "_error")) {
        switch (session->conn_state) {
            case ConnectionState_ConnectSent:
            case ConnectionState_CreateStreamSent:
            case ConnectionState_PublishSent: {
                logE_ (_func, "_error received, returning Failure");
                return Result::Failure;
            } break;
            default:
              // Ignoring
                ;
        }
    } else {
        logW_ (_func, "unknown method: ", method);
    }

    return Result::Success;
}

void
RtmpPushConnection::closed (Exception * const exc_,
                            void      * const /* _session */)
{
    if (exc_) {
        logE_ (_func, exc_->toString());
    } else
        logE_ (_func_);
}

VideoStream::Events const RtmpPushConnection::stream_events = {
    audioMessage,
    videoMessage,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */,
    mediaDesc
};

void
RtmpPushConnection::audioMessage (AudioMessage * const mt_nonnull msg,
                                  void         * const _self)
{
    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    Ref<Session> const session = self->cur_session;
    self->mutex.unlock ();

    if (!session)
        return;

    if (session->publishing.get() == 1)
        session->rtmp_conn->sendAudioMessage (msg);
}

void
RtmpPushConnection::videoMessage (VideoMessage * const mt_nonnull msg,
                                  void         * const _self)
{
    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    Ref<Session> const session = self->cur_session;
    self->mutex.unlock ();

    if (!session)
        return;

    if (session->publishing.get() == 1) {
      // TODO Wait for keyframe. Move keyframe awaiting logics
      //      from mod_rtmp to RtmpConnection.
      //      The trickier part is making this work while
      //      sending saved frames in advance.
        session->rtmp_conn->sendVideoMessage (msg);
    }
}

void
RtmpPushConnection::mediaDesc (MediaDesc_Audio *audio_desc,
                               bool             set_audio,
                               MediaDesc_Video *video_desc,
                               bool             set_video,
                               void            *_self)
{
    RtmpPushConnection * const self = static_cast <RtmpPushConnection*> (_self);

    self->mutex.lock ();
    if (self->metadata_sent) {
        self->mutex.unlock ();
        return;
    }
    self->metadata_sent = true;
    Ref<Session> const session = self->cur_session;
    self->mutex.unlock ();

    PagePool::PageListHead page_list;

    AmfAtom atoms [128];
    AmfEncoder encoder (atoms);

    encoder.addString ("@setDataFrame");
    encoder.addString ("onMetaData");

    encoder.beginObject ();

    encoder.addFieldName ("author");
    encoder.addString ("");

    encoder.addFieldName ("copyright");
    encoder.addString ("");

    encoder.addFieldName ("description");
    encoder.addString ("");

    encoder.addFieldName ("keywords");
    encoder.addString ("");

    encoder.addFieldName ("rating");
    encoder.addString ("");

    encoder.addFieldName ("title");
    encoder.addString ("");

    encoder.addFieldName ("presetname");
    encoder.addString ("Custom");

    encoder.addFieldName ("creationdate");
    encoder.addString ("Mon Oct 21 03:25:40 2013");

    if (set_video && video_desc) {
        encoder.addFieldName ("videodevice");
        encoder.addString ("Camera");

        encoder.addFieldName ("framerate");
        encoder.addNumber (25.0);

        encoder.addFieldName ("width");
        encoder.addNumber (video_desc->width);

        encoder.addFieldName ("height");
        encoder.addNumber (video_desc->height);

        if (video_desc->getType() == MediaDesc_Video::AVC) {
            MediaDesc_AVC * const avc_desc = static_cast <MediaDesc_AVC*> (video_desc);

            encoder.addFieldName ("videocodecid");
            encoder.addString ("avc1");

//#warning TODO videodatarate estimation from mod_gst - ?
            encoder.addFieldName ("videodatarate");
            encoder.addNumber (500.0);

            if (avc_desc->getCodecData().len() >= 4) {
                encoder.addFieldName ("avclevel");
                encoder.addNumber (avc_desc->getCodecData().buf() [3]);

                encoder.addFieldName ("avcprofile");
                encoder.addNumber (avc_desc->getCodecData().buf() [1]);
            }
        }

//#warning TODO videokeyframe_frequency?
        encoder.addFieldName ("videokeyframe_frequency");
        encoder.addNumber (5.0);
    }

    if (set_audio && audio_desc) {
        encoder.addFieldName ("audiodevice");
        encoder.addString ("Microphone");

        encoder.addFieldName ("audiosamplerate");
        encoder.addNumber (audio_desc->rate);

        encoder.addFieldName ("audiochannels");
        encoder.addNumber (audio_desc->channels);

        encoder.addFieldName ("audioinputvolume");
        encoder.addNumber (75.0);

//#warning TODO audiodatarate estimation from mod_gst - ?
        encoder.addFieldName ("audiodatarate");
        encoder.addNumber (64.0);
    }

    encoder.endObject ();

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_self_func, "encoder.encode() failed");
        return;
    }

    self->page_pool->getFillPages (&page_list, ConstMemory (msg_buf, msg_len));

    VideoMessage msg;
    msg.pts_nanosec = 0;
    msg.dts_nanosec = 0;
    msg.frame_type = VideoFrameType::RtmpSetMetaData;
    msg.codec_id = VideoCodecId::Unknown;

    msg.page_pool = self->page_pool;
    msg.page_list = page_list;
    msg.msg_len = msg_len;
    msg.msg_offset = 0;

    session->rtmp_conn->sendVideoMessage (&msg);
}

mt_const void
RtmpPushConnection::init (ServerThreadContext * const mt_nonnull _thread_ctx,
                          PagePool            * const mt_nonnull _page_pool,
                          VideoStream         * const _video_stream,
                          IpAddress             const _server_addr,
                          ConstMemory           const _username,
                          ConstMemory           const _password,
                          ConstMemory           const _app_name,
                          ConstMemory           const _stream_name,
                          Time                  const _ping_timeout_millisec,
                          bool                  const _momentrtmp_proto)
{
    thread_ctx = _thread_ctx;
    timers = thread_ctx->getTimers();
    page_pool = _page_pool;

    video_stream = _video_stream;

    server_addr = _server_addr;
    username = st_grab (new (std::nothrow) String (_username));
    password = st_grab (new (std::nothrow) String (_password));
    app_name = st_grab (new (std::nothrow) String (_app_name));
    stream_name = st_grab (new (std::nothrow) String (_stream_name));
    ping_timeout_millisec = _ping_timeout_millisec;
    momentrtmp_proto = _momentrtmp_proto;

    mutex.lock ();
    startNewSession (NULL /* old_session */);
    mutex.unlock ();

    video_stream->getEventInformer()->subscribe (
            CbDesc<VideoStream::Events> (&stream_events, this, this));;
}

RtmpPushConnection::RtmpPushConnection (EmbedContainer * const embed_container)
    : PushConnection   (embed_container),
      momentrtmp_proto (false),
      reconnect_timer  (NULL),
      metadata_sent    (false)
{
}

RtmpPushConnection::~RtmpPushConnection ()
{
    mutex.lock ();

    logD_ (_func, "calling deleteReconnectTimer()");
    deleteReconnectTimer ();

    if (cur_session) {
        destroySession (cur_session);
        cur_session = NULL;
    }

    mutex.unlock ();
}

mt_throws Ref<PushConnection>
RtmpPushProtocol::connect (VideoStream * const video_stream,
                           ConstMemory   const uri,
                           ConstMemory   const username,
                           ConstMemory   const password)
{
    logD_ (_func, "uri: ", uri);

    IpAddress   server_addr;
    ConstMemory app_name;
    ConstMemory stream_name;
    bool        momentrtmp_proto;
    if (!parseMomentUri (uri, 1935 /* default_port */, &server_addr, &app_name, &stream_name, &momentrtmp_proto)) {
        logE_ (_func, "Could not parse uri: ", uri);
        goto _failure;
    }
    logD_ (_func, "app_name: ", app_name, ", stream_name: ", stream_name);

  {
    Ref<RtmpPushConnection> const rtmp_push_conn = grabNewObject <RtmpPushConnection> ();
    rtmp_push_conn->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                          moment->getPagePool(),
                          video_stream,
                          server_addr,
                          username,
                          password,
                          app_name,
                          stream_name,
                          ping_timeout_millisec,
                          momentrtmp_proto);

    return rtmp_push_conn;
  }

_failure:
    exc_throw (InternalException, InternalException::BadInput);
    return NULL;
}

mt_const void
RtmpPushProtocol::init (MomentServer * const mt_nonnull moment,
                        Time           const ping_timeout_millisec)
{
    this->moment = moment;
    this->ping_timeout_millisec = ping_timeout_millisec;
}

RtmpPushProtocol::RtmpPushProtocol (EmbedContainer * const embed_container)
    : PushProtocol (embed_container),
      ping_timeout_millisec (5 * 60 * 1000)
{
}

}

