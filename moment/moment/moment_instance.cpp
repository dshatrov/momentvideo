/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>

#include <moment/moment_types.h>

#ifdef MOMENT_GPERFTOOLS
  #include <gperftools/profiler.h>
#endif

#include <moment/util_config.h>
#include <moment/inc.h>

#include <moment/moment_instance.h>


namespace Moment {

static Size  const default__page_size = 4096;
static Count const default__min_pages =  512;

static void
serverApp_threadStarted (void * const /* cb_data */)
{
  #ifdef MOMENT_GPERFTOOLS
    logD_ (_func, "calling ProfilerRegisterThread()");
    ProfilerRegisterThread ();
  #endif
}

static ServerApp::Events const server_app_events = {
    serverApp_threadStarted
};

void
MomentInstance::doExit (ConstMemory const reason)
{
    logI_ (_func, "exiting, reason: ", reason);
    server_app->stop ();
}


// ____________________________________ Ctl ____________________________________

#ifndef MOMENT_GPERFTOOLS
  static char const * const gperftools_errmsg =
          "gperftools profiler is disabled. "
          "Configure moment with --enable-gperftools and rebuild to enable.";
#endif

void
MomentInstance::ctl_startProfiler (ConstMemory const filename)
{
  #ifdef MOMENT_GPERFTOOLS
    logD_ (_func, "calling ProfilerStart()");
    ProfilerStart (String (filename).cstr());
  #else
    (void) filename;
    logD_ (_func, gperftools_errmsg);
  #endif
}

void
MomentInstance::ctl_stopProfiler ()
{
  #ifdef MOMENT_GPERFTOOLS
    logD_ (_func, "calling ProfilerStop()");
    ProfilerStop ();
    ProfilerFlush ();
  #else
    logD_ (_func, gperftools_errmsg);
  #endif
}

void
MomentInstance::ctl_exit (ConstMemory const reason)
{
    doExit (reason);
}

void
MomentInstance::ctl_abort (ConstMemory const reason)
{
    logI_ (_func, "aborting: ", reason);
    abort ();
}

void
MomentInstance::ctl_segfault (ConstMemory const reason)
{
    logI_ (_func, "segfaulting: ", reason);
    *(volatile int*)0=0; // deliberate segfault
    unreachable ();
    abort ();
}

#ifndef LIBMARY_PLATFORM_WIN32
LinePipe::Frontend const MomentInstance::ctl_pipe_frontend = {
    ctl_line,
    ctl_closed
};

void MomentInstance::ctl_line (ConstMemory   const line,
                               void        * const _self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);

    logD_ (_func, line);

    {
        ConstMemory const str = "profiler_start";
        if (line.len() >= str.len()) {
            if (equal (line.region (0, str.len()), str)) {
                self->mutex.lock ();
                Ref<MomentConfigParams> const params = self->cur_params;
                self->mutex.unlock ();

                ConstMemory profile_filename = params->profile_filename->mem();
                if (line.len() > str.len() + 1)
                    profile_filename = line.region (str.len() + 1);

                self->ctl_startProfiler (profile_filename);
                return;
            }
        }
    }

    if (equal (line, "profiler_stop")) {
        self->ctl_stopProfiler ();
        return;
    }

    if (equal (line, "exit")) {
        self->ctl_exit ("ctl");
        return;
    }

    if (equal (line, "abort")) {
        self->ctl_abort ("ctl");
        return;
    }

    if (equal (line, "segfault")) {
        self->ctl_segfault ("ctl");
        return;
    }

    if (equal (line, "reload")) {
        if (!self->initiateConfigReload ()) {
            logE_ (_func, "Could not reload config");
        }
        return;
    }

    if (equal (line, "channel_manager_start")) {
        if (!self->startChannelManager ()) {
            logE_ (_func, "startChannelManager() failed");
            self->doExit ("startChannelManager() failed");
        }
        return;
    }

    if (equal (line, "dump_clients")) {
        self->moment_server->getClientManager()->dumpAllClients();
        return;
    }

    logW_ (_func, "WARNING: Unknown control command: ", line);
}

void MomentInstance::ctl_closed (void *_self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);
    self->doExit ("fd ctl pipe closed");
}
#endif // ! defined (LIBMARY_PLATFORM_WIN32)

HttpService::HttpHandler const MomentInstance::ctl_http_handler = {
    ctlHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentInstance::ctlHttpRequest (HttpRequest   * const mt_nonnull req,
                                HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
                                IpAddress       const /* local_addr */,
                                Sender        * const mt_nonnull conn_sender,
                                Memory          const /* msg_body */,
                                void         ** const mt_nonnull /* ret_msg_data */,
                                void          * const _self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE;

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "config_reload"))
    {
        ConstMemory reply;
        if (self->initiateConfigReload ()) {
            reply = "OK";
        } else {
            logE_ (_func, "Could not reload config");
            reply = "ERROR";
        }

        conn_sender->send (
                self->moment_server->getPagePool(),
                true /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/html", reply.len() /* content_length */),
                "\r\n",
                reply);

        logA_ ("moment_ctl 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
        logE_ (_func, "Unknown admin HTTP request: ", req->getFullPath());

        ConstMemory const reply_body = "Unknown command";
        conn_sender->send (self->moment_server->getPagePool(),
                           true /* do_flush */,
                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
                           "\r\n",
                           reply_body);

        logA_ ("moment_ctl 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}


// _____________________________ Config reloading ______________________________

ConfigManager::Events const MomentInstance::config_manager_events = {
    configReload
};

void
MomentInstance::configReload (MConfig::Config * const new_config,
                              void            * const _self)
{
    MomentInstance * const self = static_cast <MomentInstance*> (_self);

    self->config_reload_mutex.lock ();

    if (logLevelOn_ (LogLevel::Debug)) {
        logLock ();
        log_locked__ (LogLevel::Debug, _func_);
        new_config->dump (logs);
        logUnlock ();
    }

    self->applyNewConfig (new_config);

    self->config_reload_mutex.unlock ();
}

Result
MomentInstance::initiateConfigReload ()
{
    Ref<MConfig::Config> const new_config = loadConfig ();
    if (!new_config) {
        logE_ (_func, "Could not load config");
        return Result::Failure;
    }

    // configReload() callback will be called
    moment_server->getConfigManager()->setConfig (new_config);

    return Result::Success;
}

Ref<MConfig::Config>
MomentInstance::loadConfig ()
{
    Ref<MConfig::Config> const new_config = grabNewObject <MConfig::Config> ();

    switch (stored_config_desc->stored_config_type) {
        case StoredConfigType_File: {
            StoredConfigDesc_File * const stored_config_desc__file =
                    static_cast <StoredConfigDesc_File*> (stored_config_desc.ptr());

            ConstMemory const real_config_filename =
                    stored_config_desc__file->config_filename.len() ?
                              stored_config_desc__file->config_filename.const_mem()
                            : ConstMemory (
                                    #if defined LIBMARY_PLATFORM_WIN32
                                      "moment.conf.txt"
                                    #elif defined LIBMARY_PLATFORM_MACOSX
                                      "/Applications/MomentVideoServer.app/moment.conf"
                                    #else
                                      "/opt/moment/moment.conf"
                                    #endif
                                      );

            logD_ (_func, "config_filename: \"", stored_config_desc__file->config_filename, "\"");
            logD_ (_func, "real_config_filename: \"", real_config_filename, "\"");

            // Solve win32 local8bit encoding first, then uncomment.
            // logI_ (_func, "config file: ", real_config_filename);

            if (!MConfig::parseConfig (real_config_filename, new_config)) {
                logE_ (_func, "could not parse config file \"", real_config_filename, "\"");
                return NULL;
            }
        } break;
        case StoredConfigType_Memory: {
            StoredConfigDesc_Memory * const stored_config_desc__memory =
                    static_cast <StoredConfigDesc_Memory*> (stored_config_desc.ptr());

            if (!MConfig::parseConfig_Memory_NoPreprocessor (stored_config_desc__memory->config_mem, new_config)) {
                logE_ (_func, "could not parse config from memory");
                return NULL;
            }
        } break;
    }

    return new_config;
}

static char const opt_name__min_pages[]               = "moment/min_pages";
static char const opt_name__page_size[]               = "moment/page_size";
static char const opt_name__num_threads[]             = "moment/num_threads";
static char const opt_name__num_file_threads[]        = "moment/num_file_threads";
static char const opt_name__profile[]                 = "moment/profile";
static char const opt_name__ctl_pipe[]                = "moment/ctl_pipe";
static char const opt_name__ctl_pipe_reopen_timeout[] = "moment/ctl_pipe_reopen_timeout";
static char const opt_name__event_bind[]              = "moment/event_bind";

Result
MomentInstance::fillParamsFromConfig (MConfig::Config    * const config,
                                      MomentConfigParams * const params)
{
    Result res = Result::Success;

    if (!configGetUint64 (config, opt_name__min_pages, &params->min_pages, default__min_pages, _func))
        res = Result::Failure;

    if (!configGetUint64 (config, opt_name__page_size, &params->page_size, default__page_size, _func))
        res = Result::Failure;

    if (!configGetUint64 (config, opt_name__num_threads, &params->num_threads, 0, _func))
        res = Result::Failure;

    if (!configGetUint64 (config, opt_name__num_file_threads, &params->num_file_threads, 0, _func))
        res = Result::Failure;

    params->profile_filename = st_grab (new (std::nothrow) String (
            config->getString_default (opt_name__profile,
                #ifdef LIBMARY_PLATFORM_MACOSX
                  "/Applications/MomentVideoServer.app/moment_profile"
                #else
                  "/opt/moment/moment_profile"
                #endif
                  )));

    params->ctl_filename = st_grab (new (std::nothrow) String (
            config->getString_default (opt_name__ctl_pipe,
                #ifdef LIBMARY_PLATFORM_MACOSX
                  "/Applications/MomentVideoServer.app/moment_ctl"
                #else
                  "/opt/moment/moment_ctl"
                #endif
                  )));

    if (!configGetUint64 (config, opt_name__ctl_pipe_reopen_timeout, &params->ctl_pipe_reopen_timeout, 1, _func))
        res = Result::Failure;

    {
        params->event_bind_valid = false;

        ConstMemory const opt_name = opt_name__event_bind;
        ConstMemory const opt_val = config->getString_default (opt_name, ConstMemory());
        logI_ (_func, opt_name, ": ", opt_val);
        if (opt_val.len()) {
            if (!setIpAddress_default (opt_val,
                                       ConstMemory() /* default_host */,
                                       14571         /* default_port */,
                                       true          /* allow_any_host */,
                                       &params->event_bind_addr))
            {
                logE_ (_func, "setIpAddress_default() failed (event)");
                res = Result::Failure;
            } else {
                params->event_bind_valid = true;
            }
        }
    }

    return res;
}

void MomentInstance::applyNewConfig (MConfig::Config * const new_config)
{
    Ref<MomentConfigParams> const params = grab (new (std::nothrow) MomentConfigParams);
    if (!fillParamsFromConfig (new_config, params)) {
        logE_ (_func, "Bad config. Server configuration has not been updated.");
        return;
    }

    mutex.lock ();
    Ref<MomentConfigParams> const old_params = cur_params;
    cur_params = params;
    mutex.unlock ();

    if (old_params && old_params->min_pages != params->min_pages)
        configWarnNoEffect (opt_name__min_pages);
#if 0
// Deprecated: PagePool::setMinPages() is gone.
    {
        logD_ (_func, "updating ", opt_name__min_pages);
        page_pool->setMinPages (params->min_pages);
    }
#endif

    if (old_params && old_params->num_threads != params->num_threads)
        configWarnNoEffect (opt_name__num_threads);

    if (old_params && old_params->num_file_threads != params->num_file_threads)
        configWarnNoEffect (opt_name__num_file_threads);

    if (old_params && !equal (old_params->profile_filename->mem(), params->profile_filename->mem()))
        logD_ (_func, opt_name__profile, " changed");

    if (old_params && !equal (old_params->ctl_filename->mem(), params->ctl_filename->mem()))
        configWarnNoEffect (opt_name__ctl_pipe);

    if (old_params && old_params->ctl_pipe_reopen_timeout != params->ctl_pipe_reopen_timeout)
        configWarnNoEffect (opt_name__ctl_pipe_reopen_timeout);

    if (old_params
        && (old_params->event_bind_valid != params->event_bind_valid
            || (params->event_bind_valid && (old_params->event_bind_addr != params->event_bind_addr))))
    {
        configWarnNoEffect (opt_name__event_bind);
    }
}

// _____________________________________________________________________________


Result
MomentInstance::startChannelManager ()
{
    mutex.lock  ();
    if (channel_manager_started) {
        mutex.unlock ();
        return Result::Success;
    }
    channel_manager_started = true;
    mutex.unlock ();

    if (!moment_server->getChannelManager()->loadConfigFull ()) {
        logE_ (_func, "channel_manager.loadConfigFull() failed");
        return Result::Failure;
    }

    return Result::Success;
}

mt_async Result
MomentInstance::run ()
{
    Result ret_res = Result::Success;

    logI_ (_func_);

    fireStarted ();

    if (!recorder_thread_pool->spawn ()) {
        logE_ (_func, "recorder_thread_pool.spawn() failed");
        return Result::Failure;
    }

    if (!reader_thread_pool->spawn ()) {
        logE_ (_func, "reader_thread_pool.spawn() failed");
        ret_res = Result::Failure;
        goto _stop_recorder;
    }

    if (!channel_manager_ctl_start) {
        if (!startChannelManager ()) {
            logE_ (_func, "tartChannelManager() failed");
            ret_res = Result::Failure;
            goto _stop_reader;
        }
    }

    if (!server_app->run ()) {
        logE_ (_func, "server_app.run() failed: ", exc->toString());
        ret_res = Result::Failure;
        goto _stop_reader;
    }

    logI_ (_func, "done");

  _stop_reader:
    reader_thread_pool->stop ();
  _stop_recorder:
    recorder_thread_pool->stop ();

    return ret_res;
}

mt_const Result
MomentInstance::init (ConfigDesc    const &config_desc,
                      LogParams     const &log_params,
                      ControlParams const &control_params)
{
    logD_ (_func, "log_filename: \"", log_params.log_filename, "\"");

    if (config_desc.config_type == ConfigType_File) {
        ConfigDesc_File const * const config_desc__file =
                static_cast <ConfigDesc_File const *> (&config_desc);
        stored_config_desc = st_grabNew <StoredConfigDesc_File> (config_desc__file->config_filename);
    } else {
        assert (config_desc.config_type == ConfigType_Memory);
        ConfigDesc_Memory const * const config_desc__memory =
                static_cast <ConfigDesc_Memory const *> (&config_desc);
        stored_config_desc = st_grabNew <StoredConfigDesc_Memory> (config_desc__memory->config_mem);
    }

    log_filename = st_grab (new (std::nothrow) String (log_params.log_filename));

    channel_manager_ctl_start = control_params.channel_manager_ctl_start;

    if (log_params.log_filename.buf()) {
        if (log_params.use_log_rotate) {
            ConstMemory const real_log_dir =
                    log_params.log_filename.len() ? log_params.log_filename : ConstMemory ("/var/log/moment");

            StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (real_log_dir);
            if (!vfs->createDirectory ("")) {
                logE_ (_this_func, "createDirectory(\"", real_log_dir, "\") failed: ", exc->toString());
            } else {
                LogRotateStream  * const stream = new (std::nothrow) LogRotateStream (NULL /* embed_container */);
                assert (stream);
                stream->init (real_log_dir,
                              "moment",
                              ".log",
                              log_params.log_max_days,
                              log_params.log_max_total_size,
                              log_params.log_max_file_size);

                logI_ (_func, "Log directory is at ", real_log_dir);

                setLogStream (stream,
                              NULL /* new_logs_release_cb */,
                              NULL /* new_logs_release_cb_data */,
                              true /* add_buffered_stream */);
            }
        } else {
            ConstMemory const real_log_filename =
                    log_params.log_filename.len() ?
                              log_params.log_filename
                            : ConstMemory ("/var/log/moment.log");
            // We never deallocate 'log_file' after log file is opened successfully.
            NativeFile * const log_file = new (std::nothrow) NativeFile (NULL /* embed_container */);
            assert (log_file);
            if (!log_file->open (real_log_filename,
                                 File::OpenFlags::Create | File::OpenFlags::Append,
                                 File::AccessMode::WriteOnly))
            {
                logE_ (_func, "Could not open log file \"", real_log_filename, "\": ", exc->toString());
                delete log_file;
            } else {
                logI_ (_func, "Log file is at ", real_log_filename);

                setLogStream (log_file,
                              NULL /* new_logs_release_cb */,
                              NULL /* new_logs_release_cb_data */,
                              true /* add_buffered_stream */);
            }
        }
    }

    MOMENT__PREINIT

    Ref<MConfig::Config> const config = loadConfig ();
    if (!config) {
        logE_ (_func, "could not load config");
        return Result::Failure;
    }

    if (logLevelOn_ (LogLevel::Debug)) {
        logLock ();
        log_locked__ (LogLevel::Debug, _func_);
        config->dump (logs);
        logUnlock ();
    }

    MOMENT__INIT

    Ref<MomentConfigParams> const params = grab (new (std::nothrow) MomentConfigParams);
    if (!fillParamsFromConfig (config, params)) {
        logE_ (_func, "Bad config. Exiting.");
        return Result::Failure;
    }
    mutex.lock ();
    cur_params = params;
    mutex.unlock ();

    server_app->getEventInformer()->subscribe (CbDesc<ServerApp::Events> (&server_app_events, NULL, NULL));
    if (!server_app->init (params->num_threads)) {
        logE_ (_func, "server_app.init() failed: ", exc->toString());
        return Result::Failure;
    }

    page_pool->init (params->page_size,
                     params->min_pages,
                     false   /* delete_immediately */,
                     server_app->getServerContext()->getMainThreadContext()->getTimers(),
                     1000000 /* 1 sec */,
                     1024    /* delete_bulk_size */);

    if (!recorder_thread_pool->init (params->num_file_threads,
                                     server_app->getServerContext()->getMainThreadContext()))
    {
        logE_ (_this_func, "recorder_thread_pool->init() failed");
        return Result::Failure;
    }

    if (!reader_thread_pool->init (params->num_file_threads /* TODO Separate config parameter? */,
                                   server_app->getServerContext()->getMainThreadContext()))
    {
        logE_ (_this_func, "reader_thread_pool->init() failed");
        return Result::Failure;
    }

    event_service->init (server_app->getServerContext(),
                         page_pool);

  #ifndef LIBMARY_PLATFORM_WIN32
    if (control_params.got_event_fd) {
        if (!event_service->addOnewayPipeFd (control_params.event_fd)) {
            logE_ (_func, "event_service.addOnewayPipeFd() failed: ", exc->toString());
            return Result::Failure;
        }
    }
  #endif

    if (params->event_bind_valid) {
        if (!event_service->initTcpServer ()) {
            logE_ (_func, "event_service.init() failed: ", exc->toString());
            return Result::Failure;
        }

        if (!event_service->bind (params->event_bind_addr)) {
            logE_ (_func, "event_service.bind() failed: ", exc->toString());
            return Result::Failure;
        }

        if (!event_service->start ()) {
            logE_ (_func, "event_service.start() failed: ", exc->toString());
            return Result::Failure;
        }
    }

    if (!moment_server->init (server_app,
                              page_pool,
                              recorder_thread_pool,
                              reader_thread_pool,
                              local_storage,
                              event_service,
                              config))
    {
        logE_ (_func, "moment_server.init() failed");
        return Result::Failure;
    }

    moment_server->getConfigManager()->getEventInformer()->subscribe (
            CbDesc<ConfigManager::Events> (&config_manager_events, this, this));

    moment_server->getHttpManager()->getAdminHttpService()->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&ctl_http_handler, this, this),
            "ctl");

  #ifndef LIBMARY_PLATFORM_WIN32
    if (String::len (params->ctl_filename)) {
        if (!line_pipe->init (params->ctl_filename->mem(),
                              CbDesc<LinePipe::Frontend> (&ctl_pipe_frontend, this, this),
                              server_app->getServerContext()->getMainThreadContext()->getPollGroup(),
                              server_app->getServerContext()->getMainThreadContext()->getDeferredProcessor(),
                              server_app->getServerContext()->getMainThreadContext()->getTimers(),
                              params->ctl_pipe_reopen_timeout * 1000 /* reopen_timeout_millisec */))
        {
            logE_ (_func, "could not initialize ctl pipe: ", exc->toString());
        }
    }

    if (control_params.got_ctl_fd) {
        if (!line_fd_pipe->init (control_params.ctl_fd,
                                 CbDesc<LineFdPipe::Frontend> (&ctl_pipe_frontend, this, this),
                                 server_app->getServerContext()->getMainThreadContext()->getPollGroup(),
                                 server_app->getServerContext()->getMainThreadContext()->getDeferredProcessor()))
        {
            logE_ (_func, "could not initialize fd ctl pipe: ", exc->toString());
            return Result::Failure;
        }
    }
  #endif

    if (!initProcessSpawner_addExitPoll (server_app->getServerContext()->getMainThreadContext()->getPollGroup())) {
        logE_ (_func, "initProcessSpawner_addExitPoll() failed");
        return Result::Failure;
    }

    return Result::Success;
}

MomentInstance::MomentInstance (EmbedContainer * const embed_container)
    : Object               (embed_container),
      event_informer       (/*outer_object=*/ this, &mutex),
      channel_manager_ctl_start (false),
      server_app           (this /* embed_container */),
      page_pool            (this /* embed_container */),
      recorder_thread_pool (this /* embed_container */),
      reader_thread_pool   (this /* embed_container */),
      local_storage        (this /* embed_container */),
      event_service        (this /* embed_container */),
      moment_server        (this /* embed_container */),
      channel_manager_started (false)
    #ifndef LIBMARY_PLATFORM_WIN32
      ,
      line_pipe            (this /* embed_container */),
      line_fd_pipe         (this /* embed_container */)
    #endif
{
}

}

