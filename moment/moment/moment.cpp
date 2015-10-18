/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>

#include <cstdio>

#ifndef LIBMARY_PLATFORM_WIN32
  #include <unistd.h>
#endif
#include <errno.h>

#ifdef MOMENT_GPERFTOOLS
  #include <gperftools/profiler.h>
#endif

#include <moment/libmoment.h>
#include <moment/moment_instance.h>
#include <moment/moment_modules.h>
#include <moment/inc.h>


using namespace M;
using namespace Moment;


namespace {
    struct Options {
        bool          help;
        bool          daemonize;
        StRef<String> config_filename;
        StRef<String> log_filename;

        bool          use_log_rotate;
        Uint64        log_max_days;
        Uint64        log_max_total_size;
        Uint64        log_max_file_size;

        LogLevel      loglevel;
        Uint64        exit_after_sec;
        Uint64        stat_interval_millisec;
        bool          gst_debug;
        bool          show_version;
        bool          channel_manager_ctl_start;

        bool          got_ctl_fd;
        int           ctl_fd;

        bool          got_event_fd;
        int           event_fd;

        Options ()
            : help                      (false),
              daemonize                 (false),
              use_log_rotate            (true),
              log_max_days              (LogRotateStream::getDefaultMaxDays()),
              log_max_total_size        (LogRotateStream::getDefaultMaxTotalSize()),
              log_max_file_size         (LogRotateStream::getDefaultMaxFileSize()),
              loglevel                  (LogLevel::Info),
              exit_after_sec            ((Uint64) -1),
              stat_interval_millisec    (0),
              gst_debug                 (false),
              show_version              (false),
              channel_manager_ctl_start (false),
              got_ctl_fd                (false),
              ctl_fd                    (-1),
              got_event_fd              (false),
              event_fd                  (-1)
        {}
    };
}

static Options options;

static void
printUsage ()
{
    outs->print ("Usage: moment [options]\n"
                  "Options:\n"
                  "  -c --config <config_file>  Configuration file to use (default: "
                        #if defined LIBMARY_PLATFORM_WIN32
                          "moment.conf.txt"
                        #elif defined LIBMARY_PLATFORM_MACOSX
                          "/Applications/MomentVideoServer.app/moment.conf"
                        #else
                          "/opt/moment/moment.conf"
                        #endif
                  "\n"
                  "  -l --log <log_file>        Log file/dir to use. "
                                                "Default: /var/log/moment, or /var/log/moment.log if --no-log-rotate.\n"
                  "  --loglevel <loglevel>      Loglevel, one of A/S/D/I/W/E/H/F/N (default: I, \"Info\")\n"
                  "  --no-log-rotate            Don't rotate logs. "
                                                "If set, then --log-max-days, --log-max-total-size and "
                                                "--log-max-file-size have no effect.\n"
                  "  --log-max-days             Maximum number of days logged.\n"
                  "  --log-max-total-size       Maximum total log size.\n"
                  "  --log-max-file-size        Maximum size of an individual log file.\n"
                #ifndef LIBMARY_PLATFORM_WIN32
                  "  -d --daemonize             Daemonize (run in the background as a daemon).\n"
                  "  --ctl-fd <fd>              Inherited cotrol pipe file descriptor number.\n"
                #endif
                  "  --exit-after <number>      Exit after specified timeout in seconds.\n"
                  "  --version                  Output version information and exit.\n"
                  "  -h --help                  Show this help message.\n");
    outs->flush ();
}

static bool
cmdline_help (char const * /* short_name */,
              char const * /* long_name */,
              char const * /* value */,
              void       * /* opt_data */,
              void       * /* cb_data */)
{
    options.help = true;
    return true;
}

static bool
cmdline_daemonize (char const * /* short_name */,
                   char const * /* long_name */,
                   char const * /* value */,
                   void       * /* opt_data */,
                   void       * /* cb_data */)
{
    options.daemonize = true;
    return true;
}

static void
cmdline_ctl_fd (char const * const value)
{
    Int64 fd_int64 = 0;
    if (!strToInt64_safe (value, &fd_int64)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --ctl-fd (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    options.got_ctl_fd = true;
    options.ctl_fd = (int) fd_int64;

    logD_ (_func, "options.ctl_fd: ", options.ctl_fd);
}

namespace {
template <class ...Args>
void badCmdlineExc (ConstMemory    const param,
                    char const   * const value,
                    Args const   &...args)
{
    logE_ (args..., "invalid value \"", value, "\" "
           "for ", param, " (number expected): ", exc->toString());
    exit (EXIT_FAILURE);
}
}

static void
cmdline_event_fd (char const * const value)
{
    Int64 fd_int64 = 0;
    if (!strToInt64_safe (value, &fd_int64))
        badCmdlineExc ("--ctl-fd", value, _func);

    options.got_event_fd = true;
    options.event_fd = (int) fd_int64;

    logD_ (_func, "options.event_fd: ", options.event_fd);
}

static void
cmdline_cman_ctl_start (char const * const /* value */)
{
    options.channel_manager_ctl_start = true;
    logD_ (_func, "options.cman_ctl_start: ", options.channel_manager_ctl_start);
}

static bool
cmdline_config (char const * /* short_name */,
                char const * /* long_name */,
                char const *value,
                void       * /* opt_data */,
                void       * /* cb_data */)
{
    options.config_filename = st_grab (new (std::nothrow) String (value));
    return true;
}

static bool
cmdline_log (char const * /* short_name */,
             char const * /* long_name */,
             char const *value,
             void       * /* opt_data */,
             void       * /* cb_data */)
{
    if (value && strlen (value))
        options.log_filename = st_grab (new (std::nothrow) String (value));

    return true;
}

static bool
cmdline_loglevel (char const * /* short_name */,
                  char const * /* long_name */,
                  char const *value,
                  void       * /* opt_data */,
                  void       * /* cb_data */)
{
    ConstMemory const value_mem = ConstMemory (value, value ? strlen (value) : 0);
    if (!LogLevel::fromString (value_mem, &options.loglevel)) {
        logE_ (_func, "Invalid loglevel name \"", value_mem, "\", using \"Info\"");
        options.loglevel = LogLevel::Info;
    }
    return true;
}

static bool
cmdline_exit_after (char const * /* short_name */,
                    char const * /* long_name */,
                    char const *value,
                    void       * /* opt_data */,
                    void       * /* cb_data */)
{
    if (!strToUint64_safe (value, &options.exit_after_sec)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --exit-after (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    logD_ (_func, "options.exit_after_sec: ", options.exit_after_sec);
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

static bool
cmdline_gst_debug (char const * /* short_name */,
                   char const * /* long_name */,
                   char const * /* value */,
                   void       * /* opt_data */,
                   void       * /* cb_data */)
{
    options.gst_debug = true;
    return true;
}

static bool
cmdline_keyfile (char const * /* short_name */,
                 char const * /* long_name */,
                 char const * const value,
                 void       * const opt_data,
                 void       * /* cb_data */)
{
    if (opt_data) {
        StRef<String> * const str = static_cast < StRef<String>* > (opt_data);
        *str = st_grab (new (std::nothrow) String (value));
    }
    return true;
}

static bool
cmdline_version (char const * /* short_name */,
                 char const * /* long_name */,
                 char const * /* value */,
                 void       * /* opt_data */,
                 void       * /* cb_data */)
{
    options.show_version = true;
    return true;
}

static void
cmdline_no_log_rotate (char const * /* value */)
{
    options.use_log_rotate = false;
}

static void
cmdline_log_max_days (char const * const value)
{
    if (!strToUint64_safe (value, &options.log_max_days)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --log-max-days (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    logD_ (_func, "options.log_max_days: ", options.exit_after_sec);
}

static void
cmdline_log_max_total_size (char const * const value)
{
    if (!strToUint64_safe (value, &options.log_max_total_size)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --log-max-total-size (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    logD_ (_func, "options.log_max_total_size: ", options.exit_after_sec);
}

static void
cmdline_log_max_file_size (char const * const value)
{
    if (!strToUint64_safe (value, &options.log_max_file_size)) {
        logE_ (_func, "Invalid value \"", value, "\" "
               "for --log-max-file-size (number expected): ", exc->toString());
        exit (EXIT_FAILURE);
    }

    logD_ (_func, "options.log_max_file_size: ", options.exit_after_sec);
}

static void statTimerTick (void * const /* cb_data */)
{
    if (logLevelOn_ (LogLevel::Debug)) {
        logLock ();
        log_locked__ (LogLevel::Debug, _func_);
        logs->print (statCountersToString());
        logUnlock ();
    }
}

static void exitTimerTick (void * const _moment_instance)
{
    MomentInstance * const moment_instance = static_cast <MomentInstance*> (_moment_instance);

    logI_ (_func, "exit timer expired (", options.exit_after_sec, " seconds)");
    moment_instance->exit ("exit timer expired");
}

int main (int argc, char **argv)
{
    libMaryInit ();

    {
        unsigned const num_opts = 17;
        CmdlineOption opts [num_opts];

        opts [0].short_name = "h";
        opts [0].long_name  = "help";
        opts [0].opt_callback = cmdline_help;

        opts [1].short_name = "d";
        opts [1].long_name  = "daemonize";
        opts [1].opt_callback = cmdline_daemonize;

        opts [2].short_name = "c";
        opts [2].long_name  = "config";
        opts [2].with_value = true;
        opts [2].opt_callback = cmdline_config;

        opts [3].short_name = "l";
        opts [3].long_name  = "log";
        opts [3].with_value = true;
        opts [3].opt_callback = cmdline_log;

        opts [4].long_name  = "loglevel";
        opts [4].with_value = true;
        opts [4].opt_callback = cmdline_loglevel;

        opts [5].long_name  = "exit-after";
        opts [5].with_value = true;
        opts [5].opt_callback = cmdline_exit_after;

        opts [6].long_name  = "gst-debug";
        opts [6].opt_callback = cmdline_gst_debug;

        opts [7].long_name  = "keyfile";
        opts [7].with_value = true;
        opts [7].opt_data   = MOMENT__KEYFILE_DATA;
        opts [7].opt_callback = cmdline_keyfile;

        opts [8].long_name  = "version";
        opts [8].opt_callback = cmdline_version;

        opts [9].long_name = "stat-interval";
        opts [9].with_value = true;
        opts [9].opt_callback = cmdline_stat_interval;

        opts [10].long_name  = "ctl-fd";
        opts [10].with_value = true;
        opts [10].callback   = cmdline_ctl_fd;

        opts [11].long_name  = "event-fd";
        opts [11].with_value = true;
        opts [11].callback   = cmdline_event_fd;

        opts [12].long_name  = "cman-ctl-start";
        opts [12].callback   = cmdline_cman_ctl_start;

        opts [13].long_name  = "no-log-rotate";
        opts [13].callback   = cmdline_no_log_rotate;

        opts [14].long_name  = "log-max-days";
        opts [14].with_value = true;
        opts [14].callback   = cmdline_log_max_days;

        opts [15].long_name  = "log-max-total-size";
        opts [15].with_value = true;
        opts [15].callback   = cmdline_log_max_total_size;

        opts [16].long_name  = "lgo-max-file-size";
        opts [16].with_value = true;
        opts [16].callback   = cmdline_log_max_file_size;

        ArrayIterator<CmdlineOption> opts_iter (opts, num_opts);
        parseCmdline (&argc, &argv, opts_iter);
    }

    if (options.show_version) {
        setGlobalLogLevel (LogLevel::Failure);
        outs->println ("moment "
                     #ifdef MOMENT_RELEASE_TAG
                       MOMENT_RELEASE_TAG
                     #else
                       "unknown"
                     #endif
                );
        return 0;
    }

    if (options.help) {
        setGlobalLogLevel (LogLevel::Failure);
        printUsage ();
        return 0;
    }

    setGlobalLogLevel (options.loglevel);

    // Note that the log file is not opened yet (logs == errs).
    if (options.daemonize) {
      #ifdef LIBMARY_PLATFORM_WIN32
        logW_ (_func, "Daemonization is not supported on Windows");
      #else
        logI_ (_func, "Daemonizing. Server log is at /var/log/moment.log");
        int const res = daemon (1 /* nochdir */, 0 /* noclose */);
        if (res == -1)
            logE_ (_func, "daemon() failed: ", errnoString (errno));
        else
        if (res != 0)
            logE_ (_func, "Unexpected return value from daemon(): ", res);
      #endif
    }

    if (!initProcessSpawner ()) {
        logE_ (_func, "initProcessSpawner() failed");
        return EXIT_FAILURE;
    }

#if 0
// Process spawning test
    for (unsigned i = 0; i < 50000; ++i) {
        StRef<String> const arg = makeString ("ECHO ", i);
        ConstMemory args [1] = { arg->mem() };
        spawnProcess ("/bin/echo", args, 1, 0, NULL, 0, NULL);
    }
#endif

  #ifdef MOMENT_GSTREAMER
    libMomentGstInit (options.gst_debug ? ConstMemory ("--gst-debug-level=5") : ConstMemory());
  #endif

    registerStatCounter (&stat_counter__page_pool_page);

    Ref<MomentInstance> const moment_instance = grabNewObject <MomentInstance> ();
    if (!moment_instance->init (MomentInstance::ConfigDesc_File (
                                        options.config_filename),
                                MomentInstance::LogParams (
                                        options.log_filename,
                                        options.use_log_rotate,
                                        options.log_max_days,
                                        options.log_max_total_size,
                                        options.log_max_file_size),
                                MomentInstance::ControlParams (
                                        options.channel_manager_ctl_start,
                                        options.got_ctl_fd,
                                        options.ctl_fd,
                                        options.got_event_fd,
                                        options.event_fd)))
    {
        logE_ (_func, "moment_instance.init() failed");
        return EXIT_FAILURE;
    }

    if (!loadModules (moment_instance,
                      moment_instance->getMomentServer()->getConfigManager()->getConfig()->getString ("moment/module_path")))
    {
        logE_ (_func, "loadModules() failed");
        return Result::Failure;
    }

    if (options.stat_interval_millisec) {
        logI_ (_func, "options.stat_interval_millisec: ", options.stat_interval_millisec);
        moment_instance->getServerApp()->getServerContext()->getMainThreadContext()->getTimers()->addTimer_microseconds (
                CbDesc<Timers::TimerCallback> (statTimerTick, NULL, NULL),
                options.stat_interval_millisec * 1000,
                true /* periodical */);
    }

    if (options.exit_after_sec != (Uint64) -1) {
        logI_ (_func, "options.exit_after_sec: ", options.exit_after_sec);
        moment_instance->getServerApp()->getServerContext()->getMainThreadContext()->getTimers()->addTimer (
                CbDesc<Timers::TimerCallback> (exitTimerTick, moment_instance, moment_instance),
                options.exit_after_sec,
                false /* periodical */);
    }

    return mt_async moment_instance->run () ? 0 : EXIT_FAILURE;
}

