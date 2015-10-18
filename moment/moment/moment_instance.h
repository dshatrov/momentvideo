/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MOMENT_INSTANCE__H__
#define MOMENT__MOMENT_INSTANCE__H__


#include <moment/moment_server.h>
#include <moment/util_config.h>


namespace Moment {

class MomentInstance : public Object
{
  private:
    StateMutex mutex;


  // _________________________________ Events __________________________________

  public:
    struct Events
    {
        typedef void Started (void *cb_data);

        Started *started;

        constexpr Events (Started * const started)
            : started (started)
        {}
    };

  private:
    Informer_<Events> event_informer;

    static void informStarted (Events * const events,
                               void   * const cb_data,
                               void   * const /* inform_data */)
    {
        if (events->started)
            events->started (cb_data);
    }

    void fireStarted ()
        { event_informer.informAll (informStarted, /*inform_data=*/ NULL); }

  public:
    Informer_<Events>* getEventInformer () { return &event_informer; }

  // ___________________________________________________________________________


    enum ConfigType {
        ConfigType_File,
        ConfigType_Memory
    };

    struct ConfigDesc
    {
        ConfigType const config_type;
        ConfigDesc (ConfigType const config_type) : config_type (config_type) {}
    };

    struct ConfigDesc_File : public ConfigDesc
    {
        ConstMemory const config_filename;

        ConfigDesc_File (ConstMemory const config_filename)
            : ConfigDesc      (ConfigType_File),
              config_filename (config_filename)
        {}
    };

    struct ConfigDesc_Memory : public ConfigDesc
    {
        ConstMemory const config_mem;

        ConfigDesc_Memory (ConstMemory const config_mem)
            : ConfigDesc (ConfigType_Memory),
              config_mem (config_mem)
        {}
    };

  private:
    enum StoredConfigType
    {
        StoredConfigType_File,
        StoredConfigType_Memory
    };

    struct StoredConfigDesc : public StReferenced
    {
        StoredConfigType const stored_config_type;
        StoredConfigDesc (StoredConfigType const stored_config_type)
            : stored_config_type (stored_config_type)
        {}
    };

    struct StoredConfigDesc_File : public StoredConfigDesc
    {
        String const config_filename;

        StoredConfigDesc_File (ConstMemory const config_filename)
            : StoredConfigDesc (StoredConfigType_File),
              config_filename  (config_filename)
        {}
    };

    struct StoredConfigDesc_Memory : public StoredConfigDesc
    {
        String const config_mem;

        StoredConfigDesc_Memory (ConstMemory const config_mem)
            : StoredConfigDesc (StoredConfigType_Memory),
              config_mem       (config_mem)
        {}
    };

    mt_const StRef<StoredConfigDesc> stored_config_desc;
    mt_const StRef<String> log_filename;

    mt_const bool channel_manager_ctl_start;

    Embed<ServerApp>       server_app;
    Embed<PagePool>        page_pool;
    Embed<FixedThreadPool> recorder_thread_pool;
    Embed<FixedThreadPool> reader_thread_pool;
    Embed<LocalStorage>    local_storage;
    Embed<EventService>    event_service;

    Embed<MomentServer>    moment_server;

    mt_mutex (mutex) bool channel_manager_started;

    void doExit (ConstMemory reason);

    static void exitTimerTick (void *_self);
    static void statTimerTick (void *_self);


  // ___________________________________ Ctl ___________________________________

    void ctl_startProfiler (ConstMemory filename);
    void ctl_stopProfiler  ();

    void ctl_exit     (ConstMemory reason);
    void ctl_abort    (ConstMemory reason);
    void ctl_segfault (ConstMemory reason);

    #ifndef LIBMARY_PLATFORM_WIN32
      Embed<LinePipe>   line_pipe;
      Embed<LineFdPipe> line_fd_pipe;

      mt_iface (LinePipe::Frontend)
        static LinePipe::Frontend const ctl_pipe_frontend;

        static void ctl_line (ConstMemory  line,
                              void        *_self);

        static void ctl_closed (void *_self);
      mt_iface_end
    #endif

    mt_iface (HttpService::HttpHandler)
      static HttpService::HttpHandler const ctl_http_handler;

      static Result ctlHttpRequest (HttpRequest   * mt_nonnull req,
                                    HttpService::HttpConnectionInfo * mt_nonnull /* conn_info */,
                                    IpAddress      local_addr,
                                    Sender        * mt_nonnull conn_sender,
                                    Memory         /* msg_body */,
                                    void         ** mt_nonnull /* ret_msg_data */,
                                    void          *_self);
    mt_iface_end


  // ____________________________ Config reloading _____________________________

    struct MomentConfigParams : public Referenced
    {
        Uint64 min_pages;
        Uint64 page_size;
        Uint64 num_threads;
        Uint64 num_file_threads;

        IpAddress event_bind_addr;
        bool      event_bind_valid;

        StRef<String> profile_filename;
        StRef<String> ctl_filename;
        Uint64        ctl_pipe_reopen_timeout;
    };

    Mutex config_reload_mutex;

    mt_mutex (mutex) Ref<MomentConfigParams> cur_params;

    mt_iface (MomentServer::Events)
      static ConfigManager::Events const config_manager_events;

      static void configReload (MConfig::Config *new_config,
                                void            *_self);
    mt_iface_end

    Result initiateConfigReload ();

    Ref<MConfig::Config> loadConfig ();

    static Result fillParamsFromConfig (MConfig::Config    *config,
                                        MomentConfigParams *params);

    void applyNewConfig (MConfig::Config *new_config);

  // ___________________________________________________________________________


    Result startChannelManager ();

  public:
    ServerApp*    getServerApp    () { return server_app; }
    PagePool*     getPagePool     () { return page_pool; }
    MomentServer* getMomentServer () { return moment_server; }

    mt_async Result run ();

    void exit (ConstMemory reason) { doExit (reason); }

  public:
    struct LogParams
    {
        ConstMemory log_filename;       // if @use_log_rotate, then this is log directory path
        bool        use_log_rotate;
        Uint64      log_max_days;       // unused if @use_log_rotate is false
        Uint64      log_max_total_size; // unused if @use_log_rotate is false
        Uint64      log_max_file_size;  // unused if @use_log_rotate is false

        LogParams ()
            : use_log_rotate     (false),
              log_max_days       (0),
              log_max_total_size (0),
              log_max_file_size  (0)
        {}

        LogParams (ConstMemory const log_filename,
                   bool        const use_log_rotate,
                   Uint64      const log_max_days,
                   Uint64      const log_max_total_size,
                   Uint64      const log_max_file_size)
            : log_filename       (log_filename),
              use_log_rotate     (use_log_rotate),
              log_max_days       (log_max_days),
              log_max_total_size (log_max_total_size),
              log_max_file_size  (log_max_file_size)
        {}
    };

    struct ControlParams
    {
        bool channel_manager_ctl_start;
        bool got_ctl_fd;
        int  ctl_fd;
        bool got_event_fd;
        int  event_fd;

        ControlParams ()
            : channel_manager_ctl_start (false),
              got_ctl_fd   (false),
              ctl_fd       (-1),
              got_event_fd (false),
              event_fd     (-1)
        {}

        ControlParams (bool const channel_manager_ctl_start,
                       bool const got_ctl_fd,
                       int  const ctl_fd,
                       bool const got_event_fd,
                       int  const event_fd)
            : channel_manager_ctl_start (channel_manager_ctl_start),
              got_ctl_fd   (got_ctl_fd),
              ctl_fd       (ctl_fd),
              got_event_fd (got_event_fd),
              event_fd     (event_fd)
        {}
    };

    mt_const Result init (ConfigDesc    const &config_desc,
                          LogParams     const &log_params,
                          ControlParams const &control_params);

    MomentInstance (EmbedContainer *embed_container);
};

template <class ...Args>
bool commonMomentModuleIsEnabled (MomentInstance * const mt_nonnull moment_instance,
                                  ConstMemory      const module_name,
                                  bool             const default_enable,
                                  Result         * const mt_nonnull ret_res,
                                  Args const     &...args)
{
    {
        bool enable = default_enable;
        if (!configGetBoolean_ (moment_instance->getMomentServer()->getConfigManager()->getConfig(),
                                makeString (module_name, "/enable"),
                                &enable,
                                args...))
        {
            *ret_res = Result::Failure;
            return false;
        }

        if (!enable) {
            logI_ (args..., module_name, " is not enabled");
            *ret_res = Result::Success;
            return false;
        }
    }

    logI_ (args..., "initializing ", module_name);

    *ret_res = Result::Success;
    return true;
}

template <class T, class ...Args>
Result commonMomentModuleInit (MomentInstance * const mt_nonnull moment_instance,
                               ConstMemory const module_name,
                               bool        const default_enable,
                               Args const &...args)
{
    {
        Result res;
        if (!commonMomentModuleIsEnabled (moment_instance, module_name, default_enable, &res, args...))
            return res;
    }

    Ref<T> const module = grabNewObject <T> ();
    if (!module->init (moment_instance->getMomentServer())) {
        logE_ (_func, "module.init() failed");
        return Result::Failure;
    }

    moment_instance->addDeletionData (module);
    return Result::Success;
}

}


#endif /* MOMENT__MOMENT_INSTANCE__H__ */

