/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/naming_scheme.h>


namespace MomentNvr {

StRef<String>
DefaultNamingScheme::getPath (ConstMemory   const channel_name,
                              Time          const unixtime_sec_,
                              Time        * const ret_next_unixtime_sec)
{
    Time const unixtime_sec = (Uint64) unixtime_sec_ + (Uint64) (offset_hours * 3600);

    struct tm tm;
    if (!unixtimeToStructTm (unixtime_sec, &tm)) {
        logE_ (_func, "unixtimeToStructTm() failed");
        return NULL;
    }

    unsigned const    day_duration = 3600 * 24;
    unsigned const   hour_duration = 3600;
    unsigned const minute_duration = 60;

    Format fmt;
    fmt.min_digits = 2;

    StRef<String> res_str;

    //  previous file    gmtime   next file boundary 
    //    boundary          |             |
    //       |              v             |
    // ______|____________________________|_____________________
    //        \____________/^
    //              |       |
    //              |   unixtime
    //              |
    //           offset

    if (file_duration_sec >= day_duration) {
        // The offset is approximate but close.
        // There may a few seconds of difference, and we don't account for that here,
        // rounding to the neares day instead.
        unsigned const offset = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;

        *ret_next_unixtime_sec = unixtime_sec + (day_duration - offset);
        res_str = makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday);
    } else
    if (file_duration_sec >= hour_duration) {
        unsigned hours = file_duration_sec / hour_duration;
        unsigned const offset = (tm.tm_hour % hours) * 3600 + tm.tm_min * 60 + tm.tm_sec;
        *ret_next_unixtime_sec = unixtime_sec + (hours * 3600 - offset);
        logD_ (_func, "tm.tm_mday: ", tm.tm_mday, ", tm.tm_hour: ", tm.tm_hour, ", hour: ", tm.tm_hour - tm.tm_hour % hours);
        res_str = makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour - tm.tm_hour % hours);
    } else
    if (file_duration_sec >= minute_duration) {
        unsigned minutes = file_duration_sec / minute_duration;
        unsigned const offset = (tm.tm_min % minutes) * 60 + tm.tm_sec;
        *ret_next_unixtime_sec = unixtime_sec + (minutes * 60 - offset);
        res_str = makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour, "/", tm.tm_min - tm.tm_min % minutes);
    } else {
        unsigned const offset = tm.tm_sec % file_duration_sec;
        *ret_next_unixtime_sec = unixtime_sec + (file_duration_sec - offset);
        res_str = makeString (tm.tm_year + 1900, "/", fmt, tm.tm_mon + 1, "/", tm.tm_mday, "/", tm.tm_hour, "/", tm.tm_min, "/", tm.tm_sec - offset);
    }

    if (*ret_next_unixtime_sec == unixtime_sec)
        *ret_next_unixtime_sec = unixtime_sec + 1;

    res_str = makeString (channel_name, "/", res_str);

    logD_ (_func,
           "unixtime_sec ", unixtime_sec, ", "
           "ret_next_unixtime_sec ", *ret_next_unixtime_sec, ", "
           "res_str \"", res_str, "\"");

    return res_str;
}

}

