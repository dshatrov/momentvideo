/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LOG__H__
#define LIBMARY__LOG__H__


#include <libmary/types.h>
#include <cstdio>

#include <libmary/memory_file.h>
#include <libmary/util_dev.h>


namespace M {

// 'logs' is an mt_unsafe object (e.g. BufferedOutputStream).
// All accesses to it must be synchronized with logLock()/logUnlock().
//
extern OutputStream *logs;

struct LogLevel
{
    enum Value {
        All     =  1000,
        Stream  =  1500,
        Debug   =  2000,
        Info    =  3000,
        Warning =  4000,
        Access  =  5000,
        Error   =  6000,
        High    =  7000,
        Failure =  8000,
        None    = 10000,
        // Short loglevel name aliases are useful to enable/disable certain
        // loglevels from source quickly. Don't use them if you don't need
        // to flip between loglevels from time to time.
        S = Stream,
        D = Debug,
        I = Info,
        W = Warning,
        A = Access,
        E = Error,
        H = High,
        F = Failure,
        N = None
    };
    operator unsigned () const { return value; }
    LogLevel (unsigned const value) : value (value) {}
    LogLevel () {}

    ConstMemory toExtStr () const;

    static Result fromString (ConstMemory  str,
                              LogLevel    * mt_nonnull ret_loglevel);

  private:
    unsigned value;
};

class LogGroup
{
  private:
    unsigned loglevel;

  public:
    void setLogLevel (unsigned const loglevel) { this->loglevel = loglevel; }
    LogLevel getLogLevel () { return loglevel; }

    LogGroup (ConstMemory const &group_name, unsigned loglevel);
};

extern LogGroup libMary_logGroup_default;
extern LogLevel libMary_globalLogLevel;

static inline void setGlobalLogLevel (LogLevel const loglevel)
    { libMary_globalLogLevel = loglevel; }

static inline LogGroup* getDefaultLogGroup () { return &libMary_logGroup_default; }
static inline unsigned  getDefaultLogLevel () { return  libMary_logGroup_default.getLogLevel (); }

static inline bool logLevelOn_ (unsigned const loglevel)
{
    return mt_unlikely (loglevel >= (unsigned) libMary_globalLogLevel &&
                        loglevel >= getDefaultLogLevel());
}

#define logLevelOn(group, loglevel)                                      \
        (mt_unlikely ((loglevel) >= (unsigned) libMary_globalLogLevel && \
                      (loglevel) >= (unsigned) libMary_logGroup_ ## group .getLogLevel()))

extern Mutex _libMary_log_mutex;
static inline void logLock   () { _libMary_log_mutex.lock   (); }
static inline void logUnlock () { _libMary_log_mutex.unlock (); }

// Note that it is possible to substitute variadic templates with a number of
// plain templates while preserving the same calling syntax.

// Note: It feels like definining *_log_* function templates as static inline
//      saves space for .so and wastes space in case of static linking.
//      It's likely better to use __attribute__(( visibility ("hidden") )).
// TODO ^ Do the same for toString(), makeString() etc.

template <class ...Args>
void _libMary_do_log_locked (LibMary_ThreadLocal * mt_nonnull tlocal,
                             OutputStream        * mt_nonnull out,
                             ConstMemory          loglevel_str,
                             Args const          &...args)
        __attribute__(( visibility ("hidden") ));

template <class ...Args>
void _libMary_do_log_locked (LibMary_ThreadLocal * const mt_nonnull tlocal,
                             OutputStream        * const mt_nonnull out,
                             ConstMemory           const loglevel_str,
                             Args const          &...args)
{
    Format fmt;
    fmt.min_digits = 2;

    Format fmt_frac;
    fmt_frac.min_digits = 4;

    out->print (
            fmt_def, tlocal->localtime.tm_year + 1900, "/", fmt, tlocal->localtime.tm_mon + 1, "/", tlocal->localtime.tm_mday, " ",
            tlocal->localtime.tm_hour, ":", tlocal->localtime.tm_min, ":", tlocal->localtime.tm_sec, ".", fmt_frac, tlocal->time_log_frac,
            loglevel_str,
            fmt_def, args..., "\n", fmt_def);
}

template <class ...Args>
void _libMary_log_locked (OutputStream * mt_nonnull out,
                          ConstMemory   loglevel_str,
                          Args const   &...args)
        __attribute__(( visibility ("hidden") ));

template <class ...Args>
void _libMary_log_locked (OutputStream * const mt_nonnull out,
                          ConstMemory    const loglevel_str,
                          Args const   &...args)
{
    exc_push_scope ();
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

    _libMary_do_log_locked (tlocal, out, loglevel_str, args...);

    out->flush ();
    exc_pop_scope ();
}

template <class ...Args>
void _libMary_log (OutputStream * mt_nonnull out,
                   ConstMemory   loglevel_str,
                   Args const   &...args)
        __attribute__(( visibility ("hidden") ));

template <class ...Args>
void _libMary_log (OutputStream * const mt_nonnull out,
                   ConstMemory    const loglevel_str,
                   Args const   &...args)
{
    logLock ();
    _libMary_log_locked (out, loglevel_str, args...);
    logUnlock ();
}

template <class ...Args>
void _libMary_logHex_locked (OutputStream * mt_nonnull out,
                             ConstMemory   loglevel_str,
                             ConstMemory   mem,
                             Args const   &...args)
        __attribute__(( visibility ("hidden") ));

template <class ...Args>
void _libMary_logHex_locked (OutputStream * const mt_nonnull out,
                             ConstMemory    const loglevel_str,
                             ConstMemory    const mem,
                             Args const   &...args)
{
    exc_push_scope ();
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

    {
        Byte buf [256];
        MemoryFile file (Memory (buf, sizeof (buf)));

        _libMary_do_log_locked (tlocal, &file, loglevel_str, args...);

        FileSize pos;
        if (mt_unlikely (!file.tell (&pos)))
            unreachable ();

        if (mt_unlikely (pos < 1 || pos > sizeof (buf)))
            unreachable ();

        hexdump (out, ConstMemory (buf, pos - 1 /* no newline */), mem);
    }

    out->flush ();
    exc_pop_scope ();
}

template <class ...Args>
void _libMary_logHex (OutputStream * mt_nonnull out,
                      ConstMemory   loglevel_str,
                      ConstMemory   mem,
                      Args const   &...args)
        __attribute__(( visibility ("hidden") ));

template <class ...Args>
void _libMary_logHex (OutputStream * const mt_nonnull out,
                      ConstMemory    const loglevel_str,
                      ConstMemory    const mem,
                      Args const   &...args)
{
    logLock ();
    _libMary_logHex_locked (out, loglevel_str, mem, args...);
    logUnlock ();
}

extern char const _libMary_loglevel_str_S [4];
extern char const _libMary_loglevel_str_D [4];
extern char const _libMary_loglevel_str_I [4];
extern char const _libMary_loglevel_str_W [4];
extern char const _libMary_loglevel_str_A [9];
extern char const _libMary_loglevel_str_E [4];
extern char const _libMary_loglevel_str_H [4];
extern char const _libMary_loglevel_str_F [4];
extern char const _libMary_loglevel_str_N [4];

// These macros allow to avoid evaluation of args when we're not going to put
// the message in the log.

#define _libMary_log_macro(out, log_func, group, loglevel, loglevel_str, ...)           \
        do {                                                                            \
            if (mt_unlikely ((loglevel) >= libMary_logGroup_ ## group .getLogLevel() && \
                             (loglevel) >= libMary_globalLogLevel))                     \
            {                                                                           \
                (log_func) ((out), (loglevel_str), __VA_ARGS__);                        \
            }                                                                           \
        } while (0)

// Double underscore __ means that no loglevel checks are made.

#define logHex(       group, loglevel, mem, ...) _libMary_log_macro (logs, _libMary_logHex,        group,   (loglevel), LogLevel (loglevel).toExtStr(), mem, __VA_ARGS__)
#define logHex_locked(group, loglevel, mem, ...) _libMary_log_macro (logs, _libMary_logHex_locked, group,   (loglevel), LogLevel (loglevel).toExtStr(), mem, __VA_ARGS__)
#define logHex_(             loglevel, mem, ...) _libMary_log_macro (logs, _libMary_logHex,        default, (loglevel), LogLevel (loglevel).toExtStr(), mem, __VA_ARGS__)
#define logHex_locked_(      loglevel, mem, ...) _libMary_log_macro (logs, _libMary_logHex_locked, default, (loglevel), LogLevel (loglevel).toExtStr(), mem, __VA_ARGS__)
#define logHex__(            loglevel, mem, ...) _libMary_logHex        (logs, LogLevel (loglevel).toExtStr(), mem, __VA_ARGS__)
#define logHex_locked__(     loglevel, mem, ...) _libMary_logHex_locked (logs, LogLevel (loglevel).toExtStr(), mem, __VA_ARGS__)

// Deprecated
//#define log__(...)        _libMary_log_macro (logs, _libMary_log,        default, LogLevel::None, _libMary_loglevel_str_N, __VA_ARGS__)
//#define log_locked__(...) _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::None, _libMary_loglevel_str_N, __VA_ARGS__)

#define log__(loglevel, ...)        _libMary_log        (logs, LogLevel (loglevel).toExtStr(), __VA_ARGS__)
#define log_locked__(loglevel, ...) _libMary_log_locked (logs, LogLevel (loglevel).toExtStr(), __VA_ARGS__)

// "log" macro name would conflict with log() library function definition.
#define log_plain( group, loglevel, ...) _libMary_log_macro (logs, _libMary_log,        group,   (loglevel), LogLevel (loglevel).toExtStr(), __VA_ARGS__)
#define log_locked(group, loglevel, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   (loglevel), LogLevel (loglevel).toExtStr(), __VA_ARGS__)
#define log_(             loglevel, ...) _libMary_log_macro (logs, _libMary_log,        default, (loglevel), LogLevel (loglevel).toExtStr(), __VA_ARGS__)
#define log_locked_(      loglevel, ...) _libMary_log_macro (logs, _libMary_log_locked, default, (loglevel), LogLevel (loglevel).toExtStr(), __VA_ARGS__)

#define logS(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::S, ConstMemory (_libMary_loglevel_str_S), __VA_ARGS__)
#define logS_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::S, ConstMemory (_libMary_loglevel_str_S), __VA_ARGS__)
#define logS_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::S, ConstMemory (_libMary_loglevel_str_S), __VA_ARGS__)
#define logS_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::S, ConstMemory (_libMary_loglevel_str_S), __VA_ARGS__)

#define logD(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::D, ConstMemory (_libMary_loglevel_str_D), __VA_ARGS__)
#define logD_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::D, ConstMemory (_libMary_loglevel_str_D), __VA_ARGS__)
#define logD_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::D, ConstMemory (_libMary_loglevel_str_D), __VA_ARGS__)
#define logD_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::D, ConstMemory (_libMary_loglevel_str_D), __VA_ARGS__)

#define logI(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::I, ConstMemory (_libMary_loglevel_str_I), __VA_ARGS__)
#define logI_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::I, ConstMemory (_libMary_loglevel_str_I), __VA_ARGS__)
#define logI_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::I, ConstMemory (_libMary_loglevel_str_I), __VA_ARGS__)
#define logI_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::I, ConstMemory (_libMary_loglevel_str_I), __VA_ARGS__)

#define logW(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::W, ConstMemory (_libMary_loglevel_str_W), __VA_ARGS__)
#define logW_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::W, ConstMemory (_libMary_loglevel_str_W), __VA_ARGS__)
#define logW_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::W, ConstMemory (_libMary_loglevel_str_W), __VA_ARGS__)
#define logW_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::W, ConstMemory (_libMary_loglevel_str_W), __VA_ARGS__)

#define logA(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::A, ConstMemory (_libMary_loglevel_str_A), __VA_ARGS__)
#define logA_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::A, ConstMemory (_libMary_loglevel_str_A), __VA_ARGS__)
#define logA_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::A, ConstMemory (_libMary_loglevel_str_A), __VA_ARGS__)
#define logA_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::A, ConstMemory (_libMary_loglevel_str_A), __VA_ARGS__)

#define logE(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::E, ConstMemory (_libMary_loglevel_str_E), __VA_ARGS__)
#define logE_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::E, ConstMemory (_libMary_loglevel_str_E), __VA_ARGS__)
#define logE_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::E, ConstMemory (_libMary_loglevel_str_E), __VA_ARGS__)
#define logE_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::E, ConstMemory (_libMary_loglevel_str_E), __VA_ARGS__)

#define logH(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::H, ConstMemory (_libMary_loglevel_str_H), __VA_ARGS__)
#define logH_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::H, ConstMemory (_libMary_loglevel_str_H), __VA_ARGS__)
#define logH_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::H, ConstMemory (_libMary_loglevel_str_H), __VA_ARGS__)
#define logH_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::H, ConstMemory (_libMary_loglevel_str_H), __VA_ARGS__)

#define logF(group, ...)        _libMary_log_macro (logs, _libMary_log,        group,   LogLevel::F, ConstMemory (_libMary_loglevel_str_F), __VA_ARGS__)
#define logF_locked(group, ...) _libMary_log_macro (logs, _libMary_log_locked, group,   LogLevel::F, ConstMemory (_libMary_loglevel_str_F), __VA_ARGS__)
#define logF_(...)              _libMary_log_macro (logs, _libMary_log,        default, LogLevel::F, ConstMemory (_libMary_loglevel_str_F), __VA_ARGS__)
#define logF_locked_(...)       _libMary_log_macro (logs, _libMary_log_locked, default, LogLevel::F, ConstMemory (_libMary_loglevel_str_F), __VA_ARGS__)

typedef void (*LogStreamReleaseCallback) (void *cb_data);

void setLogStream (OutputStream             *new_logs,
                   LogStreamReleaseCallback  new_logs_release_cb,
                   void                     *new_logs_release_cb_data,
                   bool                      add_buffered_stream);

}


#endif /* LIBMARY__LOG__H__ */

