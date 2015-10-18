/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-rtmp/rtmp_client.h>


using namespace M;

namespace Moment {

TcpConnection::Frontend const RtmpClient::tcp_conn_frontend = {
    connected
};

void
RtmpClient::connected (Exception * const exc_,
		       void      * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (exc_) {
        if (self->fetch_connection_frontend)
            self->fetch_connection_frontend.call (self->fetch_connection_frontend->disconnected,
                                                  DisconnectReason::CouldNotEstablishUnderlyingConnection);
        {
            return;
        }

        return;
    }

    logD_ (_self_func, "connected successfully");

    self->rtmp_conn->startClient ();
    self->conn_receiver->start ();
}

RtmpConnection::Backend const RtmpClient::rtmp_conn_backend = {
    closeRtmpConn
};

void
RtmpClient::closeRtmpConn (DisconnectReason   const disconnect_reason,
                           void             * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    logD_ (_self_func_);

    if (self->fetch_connection_frontend) {
        self->fetch_connection_frontend.call (self->fetch_connection_frontend->disconnected,
                                              disconnect_reason);
    }
}

RtmpConnection::Frontend const RtmpClient::rtmp_conn_frontend = {
    handshakeComplete,
    commandMessage,
    audioMessage,
    videoMessage,
    NULL /* sendStateChanged */,
    closed
};

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::handshakeComplete (void * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    logD_ (_self_func_);

    self->conn_state = ConnectionState_ConnectSent;
    self->rtmp_conn->sendConnect (self->app_name->mem(),
                                  "http://127.0.0.1/page_url",
                                  "http://127.0.0.1/swf_url",
                                  "rtmp://127.0.0.1/tc_url");

    return Result::Success;
}

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::commandMessage (bool                             const is_data,
                            MediaMessage                   * const mt_nonnull msg,
                            Uint32                           const /* msg_stream_id */,
                            AmfEncoding                      const /* amf_encoding */,
                            RtmpConnection::ConnectionInfo * const mt_nonnull /* conn_info */,
                            void                           * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

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
	logE_ (_self_func, "could not decode method name");
	return Result::Failure;
    }
    ConstMemory method (method_name, method_name_len);

    if (is_data) {
        logD_ (_self_func, "data message ignored: ", method);
        return Result::Success;
    }

    if (equal (method, "_result")) {
	switch (self->conn_state) {
	    case ConnectionState_ConnectSent: {
		self->rtmp_conn->sendCreateStream ();
		self->conn_state = ConnectionState_CreateStreamSent;
	    } break;
	    case ConnectionState_CreateStreamSent: {
		double stream_id;
		if (!decoder.decodeNumber (&stream_id)) {
		    logE_ (_self_func, "could not decode stream_id");
		    return Result::Failure;
		}

#if 0
// publish
                    self->rtmp_conn->sendPublish (options.channel->mem());

                    Ref<VideoStream> const video_stream = grab (new (std::nothrow) VideoStream);
                    video_stream->getEventInformer()->subscribe (
                            CbDesc<VideoStream::Events> (&gen_stream_handler, self, self));
#endif

                self->rtmp_conn->sendPlay (self->stream_name->mem());
		self->conn_state = ConnectionState_Streaming;
	    } break;
	    case ConnectionState_PlaySent: {
		// Unused
	    } break;
	    default:
	      // Ignoring
		;
	}
    } else
    if (equal (method, "_error")) {
	switch (self->conn_state) {
	    case ConnectionState_ConnectSent:
	    case ConnectionState_CreateStreamSent:
	    case ConnectionState_PlaySent: {
		logE_ (_self_func, "_error received, returning Failure");
		return Result::Failure;
	    } break;
	    default:
	      // Ignoring
		;
	}
    } else
    if (equal (method, "onMetaData")) {
      // No-op
    } else
    if (equal (method, "onStatus")) {
      // No-op
    } else
    if (equal (method, "|RtmpSampleAccess")) {
      // No-op
    } else {
        if (logLevelOn_ (LogLevel::Warning)) {
            logLock ();
            logW_locked_ (_self_func, "unknown method: ", method);
            PagePool::dumpPages (logs, &msg->page_list);
            logUnlock ();
        }
    }

    return Result::Success;
}

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::audioMessage (AudioMessage * const mt_nonnull audio_msg,
			  void         * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    Time const time_millisec = getTimeMilliseconds ();

    self->streamLock ();
    self->last_frame_time_millisec = time_millisec;
    self->got_first_frame = true;
    if (self->stream) {
        self->stream->fireAudioMessage_locked (audio_msg);
    }
    self->streamUnlock ();

    return Result::Success;
}

mt_sync_domain (rtmp_conn_frontend) Result
RtmpClient::videoMessage (VideoMessage * const mt_nonnull video_msg,
			  void         * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    Time const time_millisec = getTimeMilliseconds ();

    self->streamLock ();
    self->last_frame_time_millisec = time_millisec;
    self->got_first_frame = true;
    if (self->stream) {
        self->stream->fireVideoMessage_locked (video_msg);
    }
    self->streamUnlock ();

    return Result::Success;
}

void
RtmpClient::closed (Exception * const exc_,
		    void      * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (exc_)
	logD_ (_self_func, exc_->toString());
    else
	logD_ (_self_func_);

    if (self->fetch_connection_frontend) {
        self->fetch_connection_frontend.call (self->fetch_connection_frontend->disconnected,
                                              DisconnectReason::TodoCategorizeMe);
    }
}

void
RtmpClient::noVideoTimerTick (void * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    Time const time_millisec = getTimeMilliseconds ();

    self->streamLock ();

    if (!self->started) {
        self->streamUnlock ();
        return;
    }

    bool got_video = true;
    if (time_millisec >= self->last_frame_time_millisec) {
        if (self->no_video_timeout_millisec
            && time_millisec - self->last_frame_time_millisec >= self->no_video_timeout_millisec)
        {
            got_video = false;
        }
    } else {
        self->last_frame_time_millisec = time_millisec;
    }

    if (got_video) {
        if (self->got_first_frame) {
            self->streamUnlock ();

            if (self->fetch_connection_frontend)
                self->fetch_connection_frontend.call (self->fetch_connection_frontend->gotVideo, true /* got_video */);
        } else {
            self->streamUnlock ();
        }
    } else {
        self->streamUnlock ();

        if (self->fetch_connection_frontend)
            self->fetch_connection_frontend.call (self->fetch_connection_frontend->gotVideo, false /* got_video */);
    }
}

Result
RtmpClient::start ()
{
    logD_ (_this_func_);

    if (!tcp_conn->open ()) {
        logE_ (_this_func, "tcp_conn.open() failed: ", exc->toString());
        return Result::Failure;
    }

    streamLock ();
    last_frame_time_millisec = getTimeMilliseconds();
    got_first_frame = false;
    started = true;
    streamUnlock ();

    mutex.lock ();
    conn_state = ConnectionState_Connect;

    if (!thread_ctx->getPollGroup()->addPollable_beforeConnect (tcp_conn->getPollable(),
                                                                &pollable_key))
    {
        mutex.unlock ();
        logE_ (_this_func, "addPollable_beforeConnect() failed: ", exc->toString());
        return Result::Failure;
    }

    TcpConnection::ConnectResult const connect_res = tcp_conn->connect (server_addr);
    if (connect_res == TcpConnection::ConnectResult_Error) {
        if (pollable_key) {
            thread_ctx->getPollGroup()->removePollable (pollable_key);
            pollable_key = NULL;
        }
        mutex.unlock ();
	logE_ (_this_func, "tcp_conn.connect() failed: ", exc->toString());
	return Result::Failure;
    }

    if (!thread_ctx->getPollGroup()->addPollable_afterConnect (tcp_conn->getPollable(),
                                                               &pollable_key))
    {
        mutex.unlock ();
        logE_ (_this_func, "addPollable_afterConnect() failed: ", exc->toString());
        return Result::Failure;
    }

    mutex.unlock ();

    if (connect_res == TcpConnection::ConnectResult_Connected) {
        rtmp_conn->startClient ();
        conn_receiver->start ();
    } else {
        assert (connect_res == TcpConnection::ConnectResult_InProgress);
    }

    return Result::Success;
}

mt_const void
RtmpClient::init (ServerThreadContext    * const mt_nonnull thread_ctx,
                  PagePool               * const mt_nonnull page_pool,
                  MediaStream            * const stream,
                  IpAddress                const server_addr,
                  ConstMemory              const app_name,
                  ConstMemory              const stream_name,
                  RtmpClientConfig const &client_config,
                  CbDesc<FetchConnection::FetchConnectionFrontend> const &fetch_connection_frontend)
{
    this->thread_ctx       = thread_ctx;
    this->stream           = stream;
    this->server_addr      = server_addr;
    this->app_name         = st_grab (new (std::nothrow) String (app_name));
    this->stream_name      = st_grab (new (std::nothrow) String (stream_name));
    this->momentrtmp_proto = client_config.momentrtmp_proto;
    this->no_video_timeout_millisec = client_config.no_video_timeout_millisec;
    this->fetch_connection_frontend = fetch_connection_frontend;

    conn_sender->setConnection (tcp_conn);
    conn_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());

    conn_receiver->init (tcp_conn, thread_ctx->getDeferredProcessor());
    conn_receiver->setFrontend (rtmp_conn->getReceiverFrontend());

    rtmp_conn->setBackend  (CbDesc<RtmpConnection::Backend>  (&rtmp_conn_backend,  this, this));
    rtmp_conn->setFrontend (CbDesc<RtmpConnection::Frontend> (&rtmp_conn_frontend, this, this));
    rtmp_conn->setSender   (conn_sender);

    tcp_conn->setFrontend (CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, this, this));

    rtmp_conn->init (NULL /* dump_stream */,
                     thread_ctx->getTimers(),
                     page_pool,
                     client_config.send_delay_millisec,
                     client_config.ping_timeout_millisec,
                     client_config.momentrtmp_proto);

    if (client_config.no_video_check_interval_millisec) {
        thread_ctx->getTimers()->addTimer_microseconds (
                CbDesc<Timers::TimerCallback> (noVideoTimerTick, this, this),
                client_config.no_video_check_interval_millisec * 1000,
                true  /* periodical */,
                true  /* auto_delete */,
                false /* delete_after_tick */);
    }
}

RtmpClient::RtmpClient (EmbedContainer * const embed_container)
    : FetchConnection  (embed_container),
      rtmp_conn        (this /* embed_container */),
      tcp_conn         (this /* embed_container */),
      conn_sender      (this /* embed_container */),
      conn_receiver    (this /* embed_container */),
      momentrtmp_proto (false),
      last_frame_time_millisec (0),
      got_first_frame  (false),
      started          (false),
      conn_state       (ConnectionState_Initial)
{
}

RtmpClient::~RtmpClient ()
{
    mutex.lock ();
    if (pollable_key) {
        thread_ctx->getPollGroup()->removePollable (pollable_key);
    }
    mutex.unlock ();
}

}

