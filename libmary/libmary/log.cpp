/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/buffered_output_stream.h>
#include <libmary/io.h>

#include <libmary/log.h>


#define LIBMARY__LOGS_BUFFER_SIZE 4096


namespace M {

char const _libMary_loglevel_str_S [4] = " S ";
char const _libMary_loglevel_str_D [4] = " D ";
char const _libMary_loglevel_str_I [4] = " I ";
char const _libMary_loglevel_str_W [4] = " W ";
char const _libMary_loglevel_str_A [9] = " ACCESS ";
char const _libMary_loglevel_str_E [4] = " E ";
char const _libMary_loglevel_str_H [4] = " H ";
char const _libMary_loglevel_str_F [4] = " F ";
char const _libMary_loglevel_str_N [4] = " N ";

LogGroup libMary_logGroup_default ("default", LogLevel::All);
LogLevel libMary_globalLogLevel (LogLevel::All);

Mutex _libMary_log_mutex;

ConstMemory LogLevel::toExtStr () const
{
    switch (value) {
        case LogLevel::All:
            return ConstMemory (" ALL ");
        case LogLevel::Stream:
            return ConstMemory (" S ");
        case LogLevel::Debug:
            return ConstMemory (" D ");
        case LogLevel::Info:
            return ConstMemory (" I ");
        case LogLevel::Warning:
            return ConstMemory (" W ");
        case LogLevel::Access:
            return ConstMemory (" ACCESS ");
        case LogLevel::Error:
            return ConstMemory (" E ");
        case LogLevel::High:
            return ConstMemory (" H ");
        case LogLevel::Failure:
            return ConstMemory (" F ");
        case LogLevel::None:
            return ConstMemory (" N ");
    }

    unreachable ();
    return ConstMemory();
}

Result
LogLevel::fromString (ConstMemory   const str,
                      LogLevel    * const mt_nonnull ret_loglevel)
{
    LogLevel loglevel = LogLevel::Info;

    if (equal (str, "All")) {
        loglevel = LogLevel::All;
    } else
    if (equal (str, "S") || equal (str, "Stream")) {
        loglevel = LogLevel::Stream;
    } else
    if (equal (str, "D") || equal (str, "Debug")) {
        loglevel = LogLevel::Debug;
    } else
    if (equal (str, "I") || equal (str, "Info")) {
        loglevel = LogLevel::Info;
    } else
    if (equal (str, "W") || equal (str, "Warning")) {
        loglevel = LogLevel::Warning;
    } else
    if (equal (str, "A") || equal (str, "Access")) {
        loglevel = LogLevel::Access;
    } else
    if (equal (str, "E") || equal (str, "Error")) {
        loglevel = LogLevel::Error;
    } else
    if (equal (str, "H") || equal (str, "High")) {
        loglevel = LogLevel::High;
    } else
    if (equal (str, "F") || equal (str, "Failure")) {
        loglevel = LogLevel::Failure;
    } else
    if (equal (str, "N") || equal (str, "None")) {
        loglevel = LogLevel::None;
    } else {
        *ret_loglevel = LogLevel::Info;
        return Result::Failure;
    }

    *ret_loglevel = loglevel;
    return Result::Success;
}

LogGroup::LogGroup (ConstMemory const & /* group_name */,
                    unsigned    const  loglevel)
    : loglevel (loglevel)
{
    // TODO Add the group to global hash.
}

static LogStreamReleaseCallback  logs_release_cb      = NULL;
static void                     *logs_release_cb_data = NULL;
static OutputStream             *logs_buffered_stream = NULL;

void setLogStream (OutputStream             * const new_logs,
                   LogStreamReleaseCallback   const new_logs_release_cb,
                   void                     * const new_logs_release_cb_data,
                   bool                       const add_buffered_stream)
{
    logLock ();

    LogStreamReleaseCallback   const old_logs_release_cb      = logs_release_cb;
    void                     * const old_logs_release_cb_data = logs_release_cb_data;
    OutputStream             * const old_logs_buffered_stream = logs_buffered_stream;

    logs                 = new_logs;
    logs_release_cb      = new_logs_release_cb;
    logs_release_cb_data = new_logs_release_cb_data;

    if (add_buffered_stream) {
        logs_buffered_stream = new (std::nothrow) BufferedOutputStream (new_logs, LIBMARY__LOGS_BUFFER_SIZE);
        assert (logs_buffered_stream);
        logs = logs_buffered_stream;
    } else {
        logs_buffered_stream = NULL;
    }

    logUnlock ();

    if (old_logs_release_cb)
        (*old_logs_release_cb) (old_logs_release_cb_data);

    delete old_logs_buffered_stream;
}

}

