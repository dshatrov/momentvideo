/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_TIME__H__
#define LIBMARY__UTIL_TIME__H__


#include <libmary/types.h>
#include <time.h>

#include <libmary/libmary_thread_local.h>


namespace M {

// TODO 1 раз в минуту делать gettimeofday и поддерживать реальное время дня (для логов).

// Retreives cached time in seconds.
static inline Time getTime ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->time_seconds;
}

static inline Time getTimeMilliseconds ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->time_microseconds / 1000;
}

static inline Time getUnixtime ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->unixtime;
}

// Retrieves cached time in microseconds.
static inline Time getTimeMicroseconds ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    return tlocal->time_microseconds;
}

mt_throws Result updateTime ();

void splitTime (Time       unixtime,
		struct tm * mt_nonnull ret_tm);

void uSleep (Uint64 const microseconds);

void sleepSeconds (Uint64 const seconds);

static inline void sSleep (Uint64 const seconds)
{
    Uint64 const microseconds = seconds * 1000000;
    assert (microseconds > seconds);
    uSleep (microseconds);
}

Result unixtimeToStructTm (Time       unixtime,
                           struct tm * mt_nonnull ret_tm);

enum {
    unixtimeToString_BufSize = 30
};

Size unixtimeToString (Memory mem,
                       Time   time);

Size timeToHttpString (Memory     mem,
                       struct tm * mt_nonnull tm);

Result parseHttpTime (ConstMemory  mem,
                      struct tm   * mt_nonnull ret_tm);

Result parseDuration (ConstMemory  mem,
		      Time        * mt_nonnull ret_duration);

ComparisonResult compareTime (struct tm * mt_nonnull left,
                              struct tm * mt_nonnull right);

}


#endif /* LIBMARY__UTIL_TIME__H__ */

