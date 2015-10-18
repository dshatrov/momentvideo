/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/libmoment.h>

#include <moment-rtsp/rtsp_client.h>


using namespace MomentRtsp;

namespace {
class Options
{
public:
    bool help;
    LogLevel loglevel;

    Uint32 num_clients;
    Uint32 num_threads;
    Uint32 report_interval;

    bool got_play_uri;
    StRef<String> play_uri;

    bool tcp_interleaved;
    bool enable_audio;
    bool enable_video;

    Uint64 stat_interval_millisec;

    bool got_max_frames;
    Uint64 max_frames;

    bool got_exit_after;
    Uint64 exit_after;

    Options ()
        : help            (false),
          loglevel        (LogLevel::Debug),
          num_clients     (1),
          num_threads     (0),
          report_interval (0),
          got_play_uri    (false),
          tcp_interleaved (false),
          enable_audio    (true),
          enable_video    (true),
          stat_interval_millisec (0),
          got_max_frames  (false),
          max_frames      (0),
          got_exit_after  (false),
          exit_after      (0)
    {}
};
}

static mt_const Options options;

namespace {
class ClientSession : public Object
{
public:
    mt_const Byte id_char;

    Embed<RtspClient> rtsp_client;

    ClientSession (EmbedContainer * const embed_container)
        : Object      (embed_container),
          rtsp_client (this /* embed_container */)
    {}
};
}

static void rtspClientDisconnected (DisconnectReason   const /* disconnect_reason */,
                                    void             * const /* cb_data */)
{
    logE_ (_func_);
    exit (EXIT_FAILURE);
}

static void rtspClientGotVideo (bool   const got_video,
                                void * const /* cb_data */)
{
    if (!got_video) {
        logE_ (_func, "no video");
        exit (EXIT_FAILURE);
    }
}

static FetchConnection::FetchConnectionFrontend const rtsp_client_frontend (
    rtspClientDisconnected,
    rtspClientGotVideo
);

static AtomicInt report_counter; 

static void doMessage (ClientSession * const mt_nonnull session)
{
    if (options.got_max_frames || options.report_interval) {
        if (options.report_interval) {
            if ((report_counter.get () % options.report_interval) == 0) {
                logLock ();
                logs->print (ConstMemory::forObject (session->id_char));
                logs->flush ();
                logUnlock ();
            }
        }

        if (options.got_max_frames) {
            if ((Uint64) (report_counter.fetchAdd (1) + 1) >= options.max_frames)
            {
                logD_ (_func, "max_frames ", options.max_frames, " reached");
                exit (0);
            }
        } else {
            report_counter.inc ();
        }
    }
}

static void streamAudioMessage (AudioMessage * const mt_nonnull /* audio_msg */,
                                void         * const mt_nonnull _session)
{
    ClientSession * const session = static_cast <ClientSession*> (_session);
    doMessage (session);
}

static void streamVideoMessage (VideoMessage * const mt_nonnull /* video_msg */,
                                void         * const mt_nonnull _session)
{
    ClientSession * const session = static_cast <ClientSession*> (_session);
    doMessage (session);
}

static Stream::Events const stream_events = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */,
    NULL /* mediaDesc */
};

static void streamClosed (void * const /* cb_data */)
{
    logE_ (_func_);
    exit (EXIT_FAILURE);
}

static Stream::StateEvents const stream_state_events = {
    streamClosed,
    NULL /* numWatchersChanged */,
    NULL /* publishingStatusChanged */,
    NULL /* mediaDesc */
};

static Result startClients (PagePool  * const mt_nonnull page_pool,
                            ServerApp * const mt_nonnull server_app,
                            bool        const use_main_thread)
{
    Byte id_char = 'a';
    // TODO "Slow start" option.
    for (Uint32 i = 0; i < options.num_clients; ++i) {
        logD_ (_func, "Starting client, id_char: ", ConstMemory::forObject (id_char));

        // Note that ClientSession objects are never freed.
        ClientSession * const client_session = new (std::nothrow) ClientSession (NULL /* embed_container */);
        client_session->id_char = id_char;

        Ref<ServerThreadContext> thread_ctx;
        if (use_main_thread)
            thread_ctx = server_app->getServerContext()->getMainThreadContext();
        else
            thread_ctx = server_app->getServerContext()->selectThreadContext();

        ConstMemory const uri = (options.play_uri ? ConstMemory (options.play_uri->mem())
                                                  : ConstMemory ("rtsp://127.0.0.1:5554/test"));

        Ref<MediaStream> const stream = grabNewObject <MediaStream> (thread_ctx->getDeferredProcessor(),
                                                                     true  /* publishing_in_progress */,
                                                                     false /* enable_frame_saver */);
        stream->getEventInformer()->subscribe (CbDesc<Stream::Events> (
                &stream_events, client_session, client_session));
        stream->getStateEventInformer()->subscribe (CbDesc<Stream::StateEvents> (
                &stream_state_events, NULL, NULL));

        RtspClientConfig rtsp_client_config;
        rtsp_client_config.tcp_interleaved                   = options.tcp_interleaved;
        rtsp_client_config.enable_audio                      = options.enable_audio;
        rtsp_client_config.enable_video                      = options.enable_video;
        rtsp_client_config.no_avdata_check_interval_millisec = 1000;
        rtsp_client_config.no_avdata_timeout_millisec        = 10000;
        rtsp_client_config.drop_sei_nal_units                = false;

        if (!client_session->rtsp_client->init (thread_ctx,
                                                page_pool,
                                                uri,
                                                &rtsp_client_config,
                                                stream,
                                                CbDesc<FetchConnection::FetchConnectionFrontend> (&rtsp_client_frontend, NULL, NULL)))
        {
            logE_ (_func, "rtsp_client.init() failed");
            return Result::Failure;
        }

        if (!client_session->rtsp_client->start ()) {
            logE_ (_func, "rtsp_client.start() failed");
            return Result::Failure;
        }

        if (id_char == 'z')
            id_char = 'a';
        else
            ++id_char;
    }

    return Result::Success;
}

namespace {
class ClientThreadData : public Object
{
public:
    Ref<PagePool>  page_pool;
    Ref<ServerApp> server_app;

    ClientThreadData (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};
}

static void clientThreadFunc (void * const mt_nonnull _client_thread_data)
{
    ClientThreadData * const client_thread_data = static_cast <ClientThreadData*> (_client_thread_data);

    PagePool * const page_pool = client_thread_data->page_pool;
    ServerApp * const server_app = client_thread_data->server_app;

//#warning ??? What's this?
    // TODO Wait for ServerApp threads to spawn, reliably.
    logD_ (_func, "Sleeping...");
    sSleep (3);
    logD_ (_func, "Starting clients...");

    if (!startClients (page_pool, server_app, false /* use_main_thread */)) {
        logE_ (_func, "start_clients() failed");
        exit (EXIT_FAILURE);
    }

    logD_ (_func, "done");
}

namespace {
class Rtsptool : public Object
{
private:
    Embed<PagePool>  page_pool;
    Embed<ServerApp> server_app;

public:
    Result run ();

    Rtsptool (EmbedContainer * const embed_container)
        : Object     (embed_container),
          page_pool  (this /* embed_container */),
          server_app (this /* embed_container */)
    {}
};
}

static void statTimerTick (void * const /* _self */)
{
    if (logLevelOn_ (LogLevel::Debug)) {
        logLock ();
        log_locked__ (LogLevel::Debug, _func_);
        logs->print (statCountersToString());
        logUnlock ();
    }
}

static void exitTimerTick (void * const /* cb_data */)
{
    logD_ (_func, "Exit timer ", options.exit_after, " expired");
    exit (EXIT_FAILURE);
}

Result Rtsptool::run (void)
{
    if (!server_app->init (options.num_threads)) {
        logE_ (_func, "server_app.init() failed: ", exc->toString());
        return Result::Failure;
    }

    page_pool->init (4096 /* page_size */,
                     512  /* min_pages */,
                     false   /* delete_immediately */,
                     server_app->getServerContext()->getMainThreadContext()->getTimers(),
                     1000000 /* 1 sec */,
                     1024    /* delete_bulk_size */);

    if (options.stat_interval_millisec) {
        logI_ (_func, "options.stat_interval_millisec: ", options.stat_interval_millisec);
        server_app->getServerContext()->getMainThreadContext()->getTimers()->addTimer_microseconds (
                CbDesc<Timers::TimerCallback> (statTimerTick, this, this),
                options.stat_interval_millisec * 1000,
                true /* periodical */);
    }

  #ifdef LIBMARY_MT_SAFE
    Ref<Thread> client_thread;
    if (options.num_threads == 0) {
  #endif
        if (!startClients (page_pool, server_app, true /* use_main_thread */)) {
            logE_ (_func, "startClients() failed");
            return Result::Failure;
        }
  #ifdef LIBMARY_MT_SAFE
    } else {
        Ref<ClientThreadData> const client_thread_data = grabNewObject <ClientThreadData> ();
        client_thread_data->page_pool  = page_pool;
        client_thread_data->server_app = server_app;
        client_thread =
                grabNewObject <Thread> (
                        CbDesc<Thread::ThreadFunc> (clientThreadFunc,
                                                    client_thread_data,
                                                    NULL,
                                                    client_thread_data));

        if (!client_thread->spawn (true /* joinable */)) {
            logE_ (_func, "client_thread-spawn() failed");
            return Result::Failure;
        }
    }
  #endif

    if (options.got_exit_after) {
        server_app->getServerContext()->getMainThreadContext()->getTimers()->addTimer (
                CbDesc<Timers::TimerCallback> (exitTimerTick, NULL, NULL),
                options.exit_after,
                false /* periodical */,
                true  /* auto_delete */);
    }

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
    outs->print ("Usage: rtsptool [options]\n"
                 "Options:\n"
                 "  -n --num-clients <number>      Simulate N simultaneous clients. Default: 1\n"
                 "  -u --uri <URI>                 RTSP URI to connect to. Default: rtsp://127.0.0.1:5554/test\n"
                 "  --tcp                          Request TCP interleaved transport mode. Default: false\n"
                 "  --no-audio                     Don't receive audio. Default: false (do receive audio)\n"
                 "  --no-video                     Don't receive video. Default: false (do receive video)\n"
                 "  --max-frames <number>          Stop after receiving this many frames. Default: not set\n"
                 "  -t --num-threads <number>      Number of threads to spawn. Default: 0, use a single thread\n"
                 "  -r --report-interval <number>  Interval between video frame reports. Default: 0, no reports\n"
                 "  --stat-interval <number>       Statistics dump interval in milliseconds. Default: not set\n"
                 "  --exit-after <number>          Exit after specified timeout in seconds. Default: not set\n"
                 "  --loglevel <loglevel>          Loglevel (same as for 'moment' server). Default: Debug\n"
                 "  -h --help                      Show this help message.\n");
    outs->flush ();
}

static bool cmdline_help (char const * /* short_name */,
                          char const * /* long_name */,
                          char const * /* value */,
                          void       * /* opt_data */,
                          void       * /* cb_data */)
{
    options.help = true;
    return true;
}

static bool cmdline_loglevel (char const * /* short_name */,
                              char const * /* long_name */,
                              char const *value,
                              void       * /* opt_data */,
                              void       * /* cb_data */)
{

    ConstMemory const value_mem = ConstMemory  (value, (value ? strlen (value) : 0));
    if (!LogLevel::fromString (value_mem, &options.loglevel)) {
        logE_ (_func, "Invalid loglevel name \"", value_mem, "\"");
        exit (EXIT_FAILURE);
    }
    return true;
}

static bool cmdline_num_clients (char const * /* short_name */,
                                 char const * /* long_name */,
                                 char const * value,
                                 void       * /* opt_data */,
                                 void       * /* cb_data */)
{
    if (!strToUint32_safe (value, &options.num_clients)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --num-clients (number expected: ", exc->toString());
        exit (EXIT_FAILURE);
    }
    return true;
}

static bool cmdline_num_threads (char const * /* short_name */,
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

static bool cmdline_report_interval (char const * /* short_name */,
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

static bool cmdline_uri (char const * /* short_name */,
                         char const * /* long_name */,
                         char const *value,
                         void       * /* opt_data */,
                         void       * /* cb_data */)
{
    options.play_uri = makeString (value);
    return true;
}

static bool cmdline_tcp (char const * /* short_name */,
                         char const * /* long_name */,
                         char const * /* value */,
                         void       * /* opt_data */,
                         void       * /* cb_data */)
{
    options.tcp_interleaved = true;
    return true;
}

static bool cmdline_no_audio (char const * /* short_name */,
                              char const * /* long_name */,
                              char const * /* value */,
                              void       * /* opt_data */,
                              void       * /* cb_data */)
{
    options.enable_audio = false;
    return true;
}

static bool cmdline_no_video (char const * /* short_name */,
                              char const * /* long_name */,
                              char const * /* value */,
                              void       * /* opt_data */,
                              void       * /* cb_data */)
{
    options.enable_video = false;
    return true;
}

static bool
cmdline_stat_interval (char const * /* short_name */,
                       char const * /* long_name */,
                       char const *value,
                       void       * /* opt_data */,
                       void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.stat_interval_millisec)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --stat-interval (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    logD_ (_func, "options.stat_interval_millisec: ", options.stat_interval_millisec);
    return true;
}

static void
cmdline_max_frames (char const * const value)
{
    if (!strToUint64_safe (value, &options.max_frames)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --max-frames (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    options.got_max_frames = true;

    logD_ (_func, "options.max_frames: ", options.max_frames);
}

static void
cmdline_exit_after (char const * const value)
{
    if (!strToUint64_safe (value, &options.exit_after)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --exit-after (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    options.got_exit_after = true;

    logD_ (_func, "options.exit_after: ", options.exit_after);
}

int main (int argc, char **argv)
{
    libMaryInit ();

    {
        unsigned const num_opts = 12;
        CmdlineOption opts [num_opts];

        opts [0].short_name   = "h";
        opts [0].long_name    = "help";
        opts [0].opt_callback = cmdline_help;

        opts [1].long_name    = "loglevel";
        opts [1].with_value   = true;
        opts [1].opt_callback = cmdline_loglevel;

        opts [2].short_name   = "n";
        opts [2].long_name    = "num-clients";
        opts [2].with_value   = true;
        opts [2].opt_callback = cmdline_num_clients;

        opts [3].short_name   = "t";
        opts [3].long_name    = "num-threads";
        opts [3].with_value   = true;
        opts [3].opt_callback = cmdline_num_threads;

        opts [4].short_name   = "r";
        opts [4].long_name    = "report-interval";
        opts [4].with_value   = true;
        opts [4].opt_callback = cmdline_report_interval;

        opts [5].short_name   = "u";
        opts [5].long_name    = "uri";
        opts [5].with_value   = true;
        opts [5].opt_callback = cmdline_uri;

        opts [6].long_name    = "tcp";
        opts [6].opt_callback = cmdline_tcp;

        opts [7].long_name    = "no-audio";
        opts [7].opt_callback = cmdline_no_audio;

        opts [8].long_name    = "no-video";
        opts [8].opt_callback = cmdline_no_video;

        opts [9].long_name    = "stat-interval";
        opts [9].with_value   = true;
        opts [9].opt_callback = cmdline_stat_interval;

        opts [10].long_name  = "max-frames";
        opts [10].with_value = true;
        opts [10].callback   = cmdline_max_frames;

        opts [11].long_name  = "exit-after";
        opts [11].with_value = true;
        opts [11].callback   = cmdline_exit_after;

        ArrayIterator<CmdlineOption> opts_iter (opts, num_opts);
        parseCmdline (&argc, &argv, opts_iter);
    }

    if (options.help) {
        printUsage ();
        return 0;
    }

    setGlobalLogLevel (options.loglevel);

    registerStatCounter (&stat_counter__page_pool_page);
    registerStatCounter (&stat_counter__rtsp_client);

    Ref<Rtsptool> const rtsptool = grabNewObject <Rtsptool> ();
    if (rtsptool->run ())
        return 0;

    return EXIT_FAILURE;
}

