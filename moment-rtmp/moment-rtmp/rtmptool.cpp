/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cmath>

#include <moment-rtmp/rtmp_connection.h>


using namespace M;
using namespace Moment;

namespace {

LogGroup libMary_logGroup_frame_dump ("rtmptool_frame_dump", LogLevel::D /* Default: D */);

class Options
{
public:
    bool help;
    Uint32 num_clients;

    bool got_server_addr;
    IpAddress server_addr;

    bool publish;
    bool silent_publish;
    bool play;
    TestStreamGenerator::Options gen_opts;
    Ref<TestStreamGenerator> test_stream_generator;

    bool nonfatal_errors;

    StRef<String> app_name;
    StRef<String> channel;
    StRef<String> record_mode;
    // TODO Unused
    StRef<String> out_file;

    StRef<String> page_url;
    StRef<String> swf_url;
    StRef<String> tc_url;

    Uint32 num_threads;

    Uint32 report_interval;
    bool report_audio;
    bool report_video;

    bool dump_proto;
    bool dump_frames;
    bool dumphex_frames;

    LogLevel loglevel;

    Options ()
	: help            (false),
	  num_clients     (1),
	  got_server_addr (false),
          publish         (false),
          silent_publish  (false),
          play            (false),
	  nonfatal_errors (false),
	  app_name (st_grab (new (std::nothrow) String ("app"))),
	  channel  (st_grab (new (std::nothrow) String ("test"))),
          page_url (st_grab (new (std::nothrow) String ("http://127.0.0.1/page_url"))),
          swf_url  (st_grab (new (std::nothrow) String ("http://127.0.0.1/swf_url"))),
          tc_url   (st_grab (new (std::nothrow) String ("http://127.0.0.1/tc_url"))),
	  num_threads     (0),
	  report_interval (0),
          report_audio    (true),
          report_video    (true),
          dump_proto      (false),
          dump_frames     (false),
          dumphex_frames  (false),
          loglevel        (LogLevel::Debug)
    {}
};

mt_const Options options;

class RtmpClient : public Object
{
private:
    enum ConnectionState {
	ConnectionState_Connect,
	ConnectionState_ConnectSent,
	ConnectionState_CreateStreamSent,
	ConnectionState_PlaySent,
	ConnectionState_Streaming
    };

    mt_const Byte id_char;

    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const Ref<PagePool> page_pool;

    Embed<RtmpConnection>           rtmp_conn;
    Embed<TcpConnection>            tcp_conn;
    Embed<DeferredConnectionSender> conn_sender;
    Embed<ConnectionReceiver>       conn_receiver;

    // Synchronized by 'rtmp_conn_frontend'.
    ConnectionState conn_state;

    Ref<TestStreamGenerator> test_stream_generator;

    static TcpConnection::Frontend const tcp_conn_frontend;

    static void connected (Exception *exc_,
			   void      *_self);

    static RtmpConnection::Backend const rtmp_conn_backend;

    static void closeRtmpConn (DisconnectReason  disconnect_reason,
                               void             *cb_data);

  mt_iface (RtmpConnection::Frontend)
    static RtmpConnection::Frontend const rtmp_conn_frontend;

    static Result handshakeComplete (void *cb_data);

    static Result commandMessage (bool                            is_data,
                                  MediaMessage                   * mt_nonnull msg,
                                  Uint32                          msg_stream_id,
                                  AmfEncoding                     amf_encoding,
                                  RtmpConnection::ConnectionInfo * mt_nonnull conn_info,
                                  void                           *_self);

    static Result audioMessage (AudioMessage * mt_nonnull msg,
				void         *_self);

    static Result videoMessage (VideoMessage * mt_nonnull msg,
				void         *_self);

    static void closed (Exception *exc_,
			void      *_self);
  mt_iface_end

  mt_iface (MediaStream::Events)
    static MediaStream::Events const gen_stream_events;

    static void genAudioMessage (AudioMessage *audio_msg,
                                 void         *_self);

    static void genVideoMessage (VideoMessage *video_msg,
                                 void         *_self);
  mt_iface_end

public:
    Result start (IpAddress const &addr);

    mt_const void init (ServerThreadContext *thread_ctx,
			PagePool            *page_pool);

    RtmpClient (EmbedContainer *embed_container,
		Byte            id_char);
};

TcpConnection::Frontend const RtmpClient::tcp_conn_frontend = {
    connected
};

RtmpConnection::Backend const RtmpClient::rtmp_conn_backend = {
    closeRtmpConn
};

RtmpConnection::Frontend const RtmpClient::rtmp_conn_frontend = {
    handshakeComplete,
    commandMessage,
    audioMessage,
    videoMessage,
    NULL /* sendStateChanged */,
    closed
};

void
RtmpClient::connected (Exception * const exc_,
		       void      * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (exc_) {
        logE_ (_func, "exception: ", exc_->toString());
	exit (EXIT_FAILURE);
    }

    logI_ (_func, "Connected successfully");

    self->rtmp_conn->startClient ();
    self->conn_receiver->start ();
}

void
RtmpClient::closeRtmpConn (DisconnectReason   const disconnect_reason,
                           void             * const /* cb_data */)
{
    logI_ (_func, "Connection closed: disconnect_reason ", disconnect_reason);

    if (!options.nonfatal_errors)
	exit (0);
}

Result
RtmpClient::handshakeComplete (void * const _self)
{
    logD_ (_func_);

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    self->conn_state = ConnectionState_ConnectSent;
    self->rtmp_conn->sendConnect (String::mem (options.app_name),
                                  String::mem (options.page_url),
                                  String::mem (options.swf_url),
                                  String::mem (options.tc_url));

    return Result::Success;
}

Result
RtmpClient::commandMessage (bool                             const is_data,
                            MediaMessage                   * const mt_nonnull msg,
                            Uint32                           const msg_stream_id,
                            AmfEncoding                      const amf_encoding,
                            RtmpConnection::ConnectionInfo * const mt_nonnull /* conn_info */,
                            void                           * const _self)
{
    if (options.dump_frames) {
        logI (frame_dump, _func,
              "pts ", msg->pts_nanosec / 1000000, ", "
              "dts ", msg->dts_nanosec / 1000000);

        if (options.dumphex_frames) {
            logLock ();
            PagePool::dumpPages (logs, &msg->page_list, msg->msg_offset);
            logUnlock ();
	}
    }

    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (msg->msg_len == 0) {
        logD_ (_func, "msg_len 0");
	return Result::Success;
    }

    PagePool::PageListArray pl_array (msg->page_list.first, msg->msg_offset, msg->msg_len);
    AmfDecoder decoder (AmfEncoding::AMF0, &pl_array, msg->msg_len);

    if (logLevelOn_ (LogLevel::Stream)) {
        logS_ (_func, "msg dump (", (is_data ? "data" : "command"), " ", amf_encoding, "), msg_stream_id ", msg_stream_id, ":");
        decoder.dump ();
    }

    Byte method_name [256];
    Size method_name_len;
    if (!decoder.decodeString (Memory::forObject (method_name),
			       &method_name_len,
			       NULL /* ret_full_len */))
    {
	logE_ (_func, "Could not decode method name");
	return Result::Failure;
    }

    if (options.dump_frames)
        logI (frame_dump, _func, "method (", (is_data ? "data" : "command"), "): ", ConstMemory (method_name, method_name_len));

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
		    logE_ (_func, "Could not decode stream_id");
		    return Result::Failure;
		}

                if (!options.publish || options.play) {
                    self->rtmp_conn->sendPlay (options.channel->mem());
                }

                if (options.publish) {
                    self->rtmp_conn->sendPublish (options.channel->mem(),
                                                  options.record_mode ? ConstMemory (options.record_mode->mem())
                                                                      : ConstMemory ("live"));

                    if (!options.silent_publish) {
                        Ref<MediaStream> const stream =
                                grabNewObject <MediaStream> (self->thread_ctx->getDeferredProcessor(),
                                                             true  /* publishing_in_progress */,
                                                             false /* enable_frame_saver */);
                        stream->getEventInformer()->subscribe (
                                CbDesc<MediaStream::Events> (&gen_stream_events, self, self));

                        self->test_stream_generator = grabNewObject <TestStreamGenerator> ();
                        self->test_stream_generator->init (self->page_pool,
                                                           self->thread_ctx->getTimers(),
                                                           stream,
                                                           &options.gen_opts);
                        self->test_stream_generator->start ();
                    }
                }

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
		logE_ (_func, "_error received, returning Failure");
		return Result::Failure;
	    } break;
	    default:
	      // Ignoring
		;
	}
    } else
    if (equal (method, "onMetaData")) {
      // No-op

#if 0
	{
	    PagePool::Page * const page = msg->page_list->first;
	    if (page) {
                logLock ();
		hexdump (logs, page->mem());
                logUnlock ();
            }
	}
#endif
    } else
    if (equal (method, "onStatus")) {
      // No-op

#if 0
	{
	    PagePool::Page * const page = msg->page_list->first;
	    if (page) {
                logLock ();
		hexdump (logs, page->mem());
                logUnlock ();
            }
	}
#endif
    } else
    if (equal (method, "|RtmpSampleAccess")) {
      // No-op
    } else {
        logLock ();
	logW_locked_ (_func, "unknown method: ", method);
        PagePool::dumpPages (logs, &msg->page_list);
        logUnlock ();
    }

    return Result::Success;
}

static Uint32 debug_counter = 0;

Result
RtmpClient::audioMessage (AudioMessage * const mt_nonnull msg,
			  void         * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (options.dump_frames) {
        Size csum = 0;
        {
            PagePool::Page *page = msg->page_list.first;
            while (page) {
                Size i = 0;
                if (page == msg->page_list.first)
                    i = msg->msg_offset;

                for (; i < page->data_len; ++i)
                    csum += page->getData() [i];

                page = page->getNextMsgPage();
            }
        }

        logI (frame_dump, _func,
              "pts ", msg->pts_nanosec / 1000000, ", "
              "dts ", msg->dts_nanosec / 1000000, ", ",
              msg->codec_id, " ", msg->frame_type, ", rate ", msg->rate, ", ", msg->channels, " channels, len ", msg->msg_len, " csum ", fmt_hex, csum);

        if (options.dumphex_frames) {
            logLock ();
            PagePool::dumpPages (logs, &msg->page_list, msg->msg_offset);
            logUnlock ();
	}
    }

    if (options.report_audio && options.report_interval) {
	++debug_counter;
	if (debug_counter >= options.report_interval) {
	    debug_counter = 0;

            logLock ();
	    logs->print (ConstMemory::forObject (self->id_char));
	    logs->flush ();
            logUnlock ();
	}
    }

    return Result::Success;
}

Result
RtmpClient::videoMessage (VideoMessage * const mt_nonnull msg,
			  void         * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);

    if (options.dump_frames) {
        Size csum = 0;
        {
            PagePool::Page *page = msg->page_list.first;
            while (page) {
                Size i = 0;
                if (page == msg->page_list.first)
                    i = msg->msg_offset;

                for (; i < page->data_len; ++i)
                    csum += page->getData() [i];

                page = page->getNextMsgPage();
            }
        }

        logI (frame_dump, _func,
              "pts ", msg->pts_nanosec / 1000000, ", "
              "dts ", msg->dts_nanosec / 1000000, ", ",
              msg->codec_id, " ", msg->frame_type, " len ", msg->msg_len, " csum ", fmt_hex, csum);

        if (options.dumphex_frames) {
            logLock ();
            PagePool::dumpPages (logs, &msg->page_list, msg->msg_offset);
            logUnlock ();
	}
    }

    if (options.report_video && options.report_interval) {
	++debug_counter;
	if (debug_counter >= options.report_interval) {
	    debug_counter = 0;

            logLock ();
	    logs->print (ConstMemory::forObject (self->id_char));
	    logs->flush ();
            logUnlock ();
	}
    }

    return Result::Success;
}

void
RtmpClient::closed (Exception * const exc_,
		    void      * const /* _self */)
{
    if (exc_)
	logD_ (_func, exc_->toString());
    else
	logD_ (_func_);
}

MediaStream::Events const RtmpClient::gen_stream_events = {
    genAudioMessage,
    genVideoMessage,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */,
    NULL /* mediaDesc */
};

void
RtmpClient::genAudioMessage (AudioMessage * const audio_msg,
                             void         * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);
    self->rtmp_conn->sendAudioMessage (audio_msg);
}

void
RtmpClient::genVideoMessage (VideoMessage * const video_msg,
                             void         * const _self)
{
    RtmpClient * const self = static_cast <RtmpClient*> (_self);
    self->rtmp_conn->sendVideoMessage (video_msg);
}

Result
RtmpClient::start (IpAddress const &addr)
{
    if (!tcp_conn->open ()) {
        logE_ (_func, "tcp_conn.open() failed: ", exc->toString());
        return Result::Failure;
    }

    if (!thread_ctx->getPollGroup()->addPollable_beforeConnect (tcp_conn->getPollable(), NULL /* ret_pollable_key */)) {
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }

    TcpConnection::ConnectResult const connect_res = tcp_conn->connect (addr);
    if (connect_res == TcpConnection::ConnectResult_Error) {
	logE_ (_func, "tcp_conn.connect() failed: ", exc->toString());
	return Result::Failure;
    }

    if (!thread_ctx->getPollGroup()->addPollable_afterConnect (tcp_conn->getPollable(), NULL /* ret_pollable_key */)) {
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }

    if (connect_res == TcpConnection::ConnectResult_Connected) {
        rtmp_conn->startClient ();
        conn_receiver->start ();
    } else
        assert (connect_res == TcpConnection::ConnectResult_InProgress);

    return Result::Success;
}

void
RtmpClient::init (ServerThreadContext * const thread_ctx,
		  PagePool            * const page_pool)
{
    this->thread_ctx = thread_ctx;
    this->page_pool = page_pool;

    conn_sender->setConnection (tcp_conn);
    conn_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());

    conn_receiver->init (tcp_conn, thread_ctx->getDeferredProcessor());
    conn_receiver->setFrontend (rtmp_conn->getReceiverFrontend());

    rtmp_conn->setBackend  (CbDesc<RtmpConnection::Backend>  (&rtmp_conn_backend, NULL, NULL));
    rtmp_conn->setFrontend (CbDesc<RtmpConnection::Frontend> (&rtmp_conn_frontend, this, this));
    rtmp_conn->setSender (conn_sender);

    tcp_conn->setFrontend (CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, this, this));

    rtmp_conn->init ((options.dump_proto ? outs : NULL),
                     thread_ctx->getTimers(),
                     page_pool,
                     0             /* send_delay_millisec */,
                     5 * 60 * 1000 /* ping_timeout_millisec */,
                     // Note: Set to "false" for hexdumps.
                     false         /* momentrtmp_proto */);
}

RtmpClient::RtmpClient (EmbedContainer * const embed_container,
			Byte     const id_char)
    : Object        (embed_container),
      id_char       (id_char),
      rtmp_conn     (this /* embed_container */),
      tcp_conn      (this /* embed_container */),
      conn_sender   (this /* embed_container */),
      conn_receiver (this /* embed_container */),
      conn_state    (ConnectionState_Connect)
{
}

Result
startClients (PagePool  * const page_pool,
	      ServerApp * const server_app,
	      IpAddress * const server_addr,
	      bool        const use_main_thread)
{
    Byte id_char = 'a';
    // TODO "Slow start" option.
    for (Uint32 i = 0; i < options.num_clients; ++i) {
	logD_ (_func, "Starting client, id_char: ", ConstMemory::forObject (id_char));

	// Note that RtmpClient objects are never freed.
	RtmpClient * const rtmp_client = new (std::nothrow) RtmpClient (NULL /* coderef_container */, id_char);
        assert (rtmp_client);

	Ref<ServerThreadContext> thread_ctx;
	if (use_main_thread)
	    thread_ctx = server_app->getServerContext()->getMainThreadContext();
	else
	    thread_ctx = server_app->getServerContext()->selectThreadContext();

	rtmp_client->init (thread_ctx, page_pool);
	if (!rtmp_client->start (*server_addr)) {
	    logE_ (_func, "rtmp_client.start() failed");
	    return Result::Failure;
	}

	if (id_char == 'z')
	    id_char = 'a';
	else
	    ++id_char;
    }

    return Result::Success;
}

class ClientThreadData : public Object
{
public:
    Ref<PagePool>  page_pool;
    Ref<ServerApp> server_app;

    IpAddress server_addr;

    ClientThreadData (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

void clientThreadFunc (void * const _client_thread_data)
{
    ClientThreadData * const client_thread_data = static_cast <ClientThreadData*> (_client_thread_data);

    PagePool * const page_pool = client_thread_data->page_pool;
    ServerApp * const server_app = client_thread_data->server_app;

    // TODO Wait for ServerApp threads to spawn, reliably.
    logD_ (_func, "Sleeping...");
    sSleep (3);
    logD_ (_func, "Starting clients...");

    if (!startClients (page_pool, server_app, &client_thread_data->server_addr, false /* use_main_thread */))
	logE_ (_func, "start_clients() failed");

    logD_ (_func, "done");
}

class RtmptoolInstance : public Object
{
private:
    Embed<PagePool>  page_pool;
    Embed<ServerApp> server_app;

public:
    Result run ();

    RtmptoolInstance (EmbedContainer * const embed_container)
        : Object     (embed_container),
          page_pool  (this /* embed_container */),
          server_app (this /* embed_container */)
    {
        page_pool->init (4096 /* page_size */,
                         4096 /* min_pages */,
                         true /* delete_immediately */,
                         NULL /* timers */,
                         0    /* delete_interval_microsec */,
                         0    /* delete_bulk_size */);
    }
};

Result
RtmptoolInstance::run (void)
{
    // TODO Check overall MT safety of rtmptool.
    if (!server_app->init (options.num_threads)) {
	logE_ (_func, "server_app.init() failed: ", exc->toString());
	return Result::Failure;
    }

    IpAddress server_addr = options.server_addr;
    if (!options.got_server_addr) {
	if (!setIpAddress ("localhost:1935", &server_addr)) {
	    logE_ (_func, "setIpAddress() failed");
	    return Result::Failure;
	}
    }

#ifdef LIBMARY_MT_SAFE
    Ref<Thread> client_thread;
    if (options.num_threads == 0) {
#endif
	startClients (page_pool, server_app, &server_addr, true /* use_main_thread */);
#ifdef LIBMARY_MT_SAFE
    } else {
	Ref<ClientThreadData> const client_thread_data = grabNewObject <ClientThreadData> ();
	client_thread_data->page_pool = page_pool;
	client_thread_data->server_app = server_app;
	client_thread_data->server_addr = server_addr;
	client_thread =
                grabNewObject <Thread> (
                        CbDesc<Thread::ThreadFunc> (clientThreadFunc,
                                                    client_thread_data /* cb_data */,
                                                    NULL /* coderef_container */,
                                                    client_thread_data /* ref_data */));

	if (!client_thread->spawn (true /* joinable */)) {
	    logE_ (_func, "client_thread->spawn() failed");
	    return Result::Failure;
	}
    }
#endif

    logI_ (_func, "Starting...");
    if (!server_app->run ()) {
	logE_ (_func, "server_app.run() failed: ", exc->toString());
    }
    logI_ (_func, "...Finished");

#ifdef LIBMARY_MT_SAFE
    if (client_thread) {
	if (!client_thread->join ())
	    logE_ (_func, "client_thread.join() failed: ", exc->toString());
    }
#endif

    return Result::Success;
}

static void printUsage ()
{
    outs->print ("Usage: rtmptool [options]\n"
                 "Options:\n"
                 "  -n --num-clients <number>      Simulate N simultaneous clients. Default: 1\n"
                 "  -s --server-addr <address>     Server address, IP:PORT. Default: localhost:1935\n"
                 "  -a --app <string>              Application name. Default: app\n"
                 "  -c --channel <string>          Name of the channel to subscribe to. Default: video\n"
                 "  -p --publish                   Publish a dummy stream instead of playing it.\n"
                 "  --record-mode <string>         Recording mode (record/append/live).\n"
                 "  --silent-publish               Don't sent and audio/video data when publishing.\n"
                 "  --play                         When publishing, play the stream as well.\n"
                 "  --page-url <string>            pageUrl to send with RTMP \"connect\" command.\n"
                 "  --swf-url  <string>            swfUrl  to send with RTMP \"connect\" command.\n"
                 "  --tc-url   <string>            tcUrl   to send with RTMP \"connect\" command.\n"
                 "  --frame-duration               Duration of a single frame in milliseconds. Default: 40 (25 frames per second)\n"
                 "  --frame-size                   Frame size in bytes. Default: 2500 bytes.\n"
                 "  --start-timestamp              Timestamp of the first generated video message. Default: 0\n"
                 "  --keyframe-interval            Distance between keyframes, in frames. Default: 10 frames\n"
                 "  --burst-width                  Number of frames to generate in a single iteration. Default: 1\n"
                 "  -t --num-threads <number>      Number of threads to spawn. Default: 0, use a single thread\n"
                 "  -d --dump-frames               Dump incoming messages.\n"
                 "  -x --dump-hex                  Print hexdumps when dumping incoming messages.\n"
                 "  --dump-proto                   RTMP protocol dump.\n"
                 "  -r --report-interval <number>  Interval between video frame reports. Default: 0, no reports\n"
                 "  --nonfatal-errors              Do not exit on the first error.\n"
                 "  --loglevel <loglevel>          Loglevel (same as for 'moment' server).\n"
                 "  -h --help                      Show this help message.\n"
//                 "  -o --out-file - Output file name.\n"
                 );
    outs->flush ();
}

bool cmdline_help (char const * /* short_name */,
		   char const * /* long_name */,
		   char const * /* value */,
		   void       * /* opt_data */,
		   void       * /* cb_data */)
{
    options.help = true;
    return true;
}

bool cmdline_num_clients (char const * /* short_name */,
			  char const * /* long_name */,
			  char const *value,
			  void       * /* opt_data */,
			  void       * /* cb_data */)
{
    if (!strToUint32_safe (value, &options.num_clients)) {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --num-clients (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_server_addr (char const * /* short_name */,
			  char const * /* long_name */,
			  char const *value,
			  void       * /* opt_data */,
			  void       * /* cb_data */)
{
    if (!setIpAddress_default (ConstMemory (value, (value ? strlen (value) : 0)),
			       "localhost",
			       1935  /* default_port */,
			       false /* allow_any_host */,
			       &options.server_addr))
    {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --server-addr (IP:PORT expected)");
	exit (EXIT_FAILURE);
    }
    options.got_server_addr = true;
    return true;
}

bool cmdline_app (char const * /* short_name */,
		  char const * /* long_name */,
		  char const *value,
		  void       * /* opt_data */,
		  void       * /* cb_data */)
{
    options.app_name = st_grab (new (std::nothrow) String (value));
    return true;
}

bool cmdline_channel (char const * /* short_name */,
		      char const * /* long_name */,
		      char const *value,
		      void       * /* opt_data */,
		      void       * /* cb_data */)
{
    options.channel = st_grab (new (std::nothrow) String (value));
    return true;
}

bool cmdline_out_file (char const * /* short_name */,
		       char const * /* long_name */,
		       char const *value,
		       void       * /* opt_data */,
		       void       * /* cb_data */)
{
    options.out_file = st_grab (new (std::nothrow) String (value));
    return true;
}

bool cmdline_page_url (char const * /* short_name */,
                       char const * /* long_name */,
                       char const *value,
                       void       * /* opt_name */,
                       void       * /* cb_data */)
{
    options.page_url = st_grab (new (std::nothrow) String (value));
    return true;
}

bool cmdline_swf_url (char const * /* short_name */,
                      char const * /* long_name */,
                      char const *value,
                      void       * /* opt_name */,
                      void       * /* cb_data */)
{
    options.swf_url = st_grab (new (std::nothrow) String (value));
    return true;
}

bool cmdline_tc_url (char const * /* short_name */,
                     char const * /* long_name */,
                     char const *value,
                     void       * /* opt_name */,
                     void       * /* cb_data */)
{
    options.tc_url = st_grab (new (std::nothrow) String (value));
    return true;
}

bool cmdline_num_threads (char const * /* short_name */,
			  char const * /* long_name */,
			  char const *value,
			  void       * /* opt_data */,
			  void       * /* cb_data */)
{
    if (!strToUint32_safe (value, &options.num_threads)) {
 	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --num-threads (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_report_interval (char const * /* short_name */,
			      char const * /* long_name */,
			      char const *value,
			      void       * /* opt_data */,
			      void       * /* cb_data */)
{
    if (!strToUint32_safe (value, &options.report_interval)) {
	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --report-interval (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_report_audio (char const * /* short_name */,
			   char const * /* long_name */,
			   char const * /* value */,
			   void       * /* opt_data */,
			   void       * /* cb_data */)
{
    options.report_audio = true;
    return true;
}

bool cmdline_report_video (char const * /* short_name */,
			   char const * /* long_name */,
			   char const * /* value */,
			   void       * /* opt_data */,
			   void       * /* cb_data */)
{
    options.report_video = true;
    return true;
}

bool cmdline_nonfatal_errors (char const * /* short_name */,
			      char const * /* long_name */,
			      char const * /* value */,
			      void       * /* opt_data */,
			      void       * /* cb_data */)
{
    options.nonfatal_errors = true;
    return true;
}

bool cmdline_dump_proto (char const * /* short_name */,
                         char const * /* long_name */,
                         char const * /* value */,
                         void       * /* opt_data */,
                         void       * /* cb_data */)
{
    options.dump_proto = true;
    return true;
}

bool cmdline_dump_frames (char const * /* short_name */,
                          char const * /* long_name */,
                          char const * /* value */,
                          void       * /* opt_data */,
                          void       * /* cb_data */)
{
    options.dump_frames = true;
    return true;
}

bool cmdline_dumphex_frames (char const * /* short_name */,
                             char const * /* long_name */,
                             char const * /* value */,
                             void       * /* opt_data */,
                             void       * /* cb_data */)
{
    options.dumphex_frames = true;
    return true;
}

bool cmdline_publish (char const * /* short_name */,
                      char const * /* long_name */,
                      char const * /* value */,
                      void       * /* opt_data */,
                      void       * /* cb_data */)
{
    options.publish = true;
    return true;
}

bool cmdline_silent_publish (char const * /* short_name */,
                             char const * /* long_name */,
                             char const * /* value */,
                             void       * /* opt_data */,
                             void       * /* cb_data */)
{
    options.silent_publish = true;
    return true;
}

bool cmdline_play (char const * /* short_name */,
                   char const * /* long_name */,
                   char const * /* value */,
                   void       * /* opt_data */,
                   void       * /* cb_data */)
{
    options.play = true;
    return true;
}

bool cmdline_frame_duration (char const * /* short_name */,
                             char const * const long_name,
                             char const * const value,
                             void       * /* opt_data */,
                             void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.gen_opts.frame_duration)) {
 	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --", long_name, " (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_frame_size (char const * /* short_name */,
                         char const * const long_name,
                         char const * const value,
                         void       * /* opt_data */,
                         void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.gen_opts.frame_size)) {
 	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --", long_name, " (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_start_timestamp (char const * /* short_name */,
                              char const * const long_name,
                              char const * const value,
                              void       * /* opt_data */,
                              void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.gen_opts.start_timestamp)) {
 	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --", long_name, " (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_keyframe_interval (char const * /* short_name */,
                                char const * const long_name,
                                char const * const value,
                                void       * /* opt_data */,
                                void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.gen_opts.keyframe_interval)) {
 	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --", long_name, " (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

bool cmdline_burst_width (char const * /* short_name */,
                          char const * const long_name,
                          char const * const value,
                          void       * /* opt_data */,
                          void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.gen_opts.burst_width)) {
 	logE_ (_func, "Invalid value \"", value, "\" "
	       "for --", long_name, " (number expected): ", exc->toString());
	exit (EXIT_FAILURE);
    }
    return true;
}

static bool
cmdline_loglevel (char const * /* short_name */,
                  char const * /* long_name */,
                  char const * const value,
                  void       * /* opt_data */,
                  void       * /* cb_data */)
{
    ConstMemory const value_mem = ConstMemory (value, (value ? strlen (value) : 0));
    if (!LogLevel::fromString (value_mem, &options.loglevel)) {
        logE_ (_func, "Invalid loglevel name \"", value_mem, "\", using \"Info\"");
        options.loglevel = LogLevel::Info;
    }
    return true;
}

bool cmdline_record_mode (char const * /* short_name */,
                          char const * /* long_name */,
                          char const *value,
                          void       * /* opt_name */,
                          void       * /* cb_data */)
{
    options.record_mode = st_grab (new (std::nothrow) String (value));
    return true;
}

} // namespace {}

int main (int argc, char **argv)
{
    libMaryInit ();

    {
	unsigned const num_opts = 27;
	CmdlineOption opts [num_opts];

	opts [0].short_name = "h";
	opts [0].long_name  = "help";
	opts [0].with_value = false;
	opts [0].opt_data   = NULL;
	opts [0].opt_callback = cmdline_help;

	opts [1].short_name = "n";
	opts [1].long_name  = "num-clients";
	opts [1].with_value = true;
	opts [1].opt_data   = NULL;
	opts [1].opt_callback = cmdline_num_clients;

	opts [2].short_name = "s";
	opts [2].long_name  = "server-addr";
	opts [2].with_value = true;
	opts [2].opt_data   = NULL;
	opts [2].opt_callback = cmdline_server_addr;

	opts [3].short_name = "a";
	opts [3].long_name  = "app";
	opts [3].with_value = true;
	opts [3].opt_data   = NULL;
	opts [3].opt_callback = cmdline_app;

	opts [4].short_name = "c";
	opts [4].long_name  = "channel";
	opts [4].with_value = true;
	opts [4].opt_data   = NULL;
	opts [4].opt_callback = cmdline_channel;

	opts [5].short_name = "o";
	opts [5].long_name  = "out-file";
	opts [5].with_value = true;
	opts [5].opt_data   = NULL;
	opts [5].opt_callback = cmdline_out_file;

	opts [6].short_name = "r";
	opts [6].long_name  = "report-interval";
	opts [6].with_value = true;
	opts [6].opt_data   = NULL;
	opts [6].opt_callback = cmdline_report_interval;

	opts [7].short_name = NULL;
	opts [7].long_name  = "nonfatal-errors";
	opts [7].with_value = false;
	opts [7].opt_data   = NULL;
	opts [7].opt_callback = cmdline_nonfatal_errors;

	opts [8].short_name = "t";
	opts [8].long_name  = "num-threads";
	opts [8].with_value = true;
	opts [8].opt_data   = NULL;
	opts [8].opt_callback = cmdline_num_threads;

        opts [9].short_name = "d";
        opts [9].long_name  = "dump-frames";
        opts [9].with_value = false;
        opts [9].opt_data   = NULL;
        opts [9].opt_callback = cmdline_dump_frames;

        opts [10].short_name = "p";
        opts [10].long_name  = "publish";
        opts [10].with_value = false;
        opts [10].opt_data   = NULL;
        opts [10].opt_callback = cmdline_publish;

        opts [11].short_name = NULL;
        opts [11].long_name  = "frame-duration";
        opts [11].with_value = true;
        opts [11].opt_data   = NULL;
        opts [11].opt_callback = cmdline_frame_duration;

        opts [12].short_name = NULL;
        opts [12].long_name  = "frame-size";
        opts [12].with_value = true;
        opts [12].opt_data   = NULL;
        opts [12].opt_callback = cmdline_frame_size;

        opts [13].short_name = NULL;
        opts [13].long_name  = "start-timestamp";
        opts [13].with_value = true;
        opts [13].opt_data   = NULL;
        opts [13].opt_callback = cmdline_start_timestamp;

        opts [14].short_name = NULL;
        opts [14].long_name  = "keyframe-interval";
        opts [14].with_value = true;
        opts [14].opt_data   = NULL;
        opts [14].opt_callback = cmdline_keyframe_interval;

        opts [15].short_name = NULL;
        opts [15].long_name  = "burst-width";
        opts [15].with_value = true;
        opts [15].opt_data   = NULL;
        opts [15].opt_callback = cmdline_burst_width;

        opts [16].short_name = NULL;
        opts [16].long_name  = "loglevel";
        opts [16].with_value = true;
        opts [16].opt_data   = NULL;
        opts [16].opt_callback = cmdline_loglevel;

        opts [17].short_name = NULL;
        opts [17].long_name  = "play";
        opts [17].with_value = false;
        opts [17].opt_data   = NULL;
        opts [17].opt_callback = cmdline_play;

        opts [18].short_name   = NULL;
        opts [18].long_name    = "report-audio";
        opts [18].with_value   = false;
        opts [18].opt_data     = NULL;
        opts [18].opt_callback = cmdline_report_audio;

        opts [19].short_name   = NULL;
        opts [19].long_name    = "report-video";
        opts [19].with_value   = false;
        opts [19].opt_data     = NULL;
        opts [19].opt_callback = cmdline_report_video;

        opts [20].short_name = "x";
        opts [20].long_name  = "dump-hex";
        opts [20].with_value = false;
        opts [20].opt_data   = NULL;
        opts [20].opt_callback = cmdline_dumphex_frames;

        opts [21].short_name   = NULL;
        opts [21].long_name    = "page-url";
        opts [21].with_value   = true;
        opts [21].opt_data     = NULL;
        opts [21].opt_callback = cmdline_page_url;

        opts [22].short_name   = NULL;
        opts [22].long_name    = "swf-url";
        opts [22].with_value   = true;
        opts [22].opt_data     = NULL;
        opts [22].opt_callback = cmdline_swf_url;

        opts [23].short_name   = NULL;
        opts [23].long_name    = "tc-url";
        opts [23].with_value   = true;
        opts [23].opt_data     = NULL;
        opts [23].opt_callback = cmdline_tc_url;

        opts [24].short_name = NULL;
        opts [24].long_name  = "silent-publish";
        opts [24].with_value = false;
        opts [24].opt_data   = NULL;
        opts [24].opt_callback = cmdline_silent_publish;

        opts [25].short_name = NULL;
        opts [25].long_name  = "dump-proto";
        opts [25].with_value = false;
        opts [25].opt_data   = NULL;
        opts [25].opt_callback = cmdline_dump_proto;

        opts [26].short_name = NULL;
        opts [26].long_name  = "record-mode";
        opts [26].with_value = true;
        opts [26].opt_data   = NULL;
        opts [26].opt_callback = cmdline_record_mode;

	ArrayIterator<CmdlineOption> opts_iter (opts, num_opts);
	parseCmdline (&argc, &argv, opts_iter, NULL /* callback */, NULL /* callback_data */);
    }

    if (options.help) {
	printUsage ();
	return 0;
    }

    setGlobalLogLevel (options.loglevel);

    Ref<RtmptoolInstance> const rtmptool_instance = grabNewObject <RtmptoolInstance> ();
    if (rtmptool_instance->run ())
	return 0;

    return EXIT_FAILURE;
}

