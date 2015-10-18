/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/rate_limit.h>


namespace Moment {

static LogGroup libMary_logGroup_ratelimit ("ratelimit", LogLevel::D);

void
RateLimitParams::dump (LogLevel    const loglevel,
                       ConstMemory const prefix)
{
    log__ (loglevel, prefix, "realtime: ",                    realtime);
    log__ (loglevel, prefix, "min_frame_duration_microsec: ", min_frame_duration_microsec);
    log__ (loglevel, prefix, "bytes_per_second: ",            bytes_per_second);
    log__ (loglevel, prefix, "burst_duration_microsec: ",     burst_duration_microsec);
    log__ (loglevel, prefix, "max_burst_size: ",              max_burst_size);
    log__ (loglevel, prefix, "prepush_duration_microsec: ",   prepush_duration_microsec);
    log__ (loglevel, prefix, "prepush_size: ",                prepush_size);
}

RateLimitParams::RateLimitParams ()
    : realtime                    (true),
      min_frame_duration_microsec (1000),
      bytes_per_second            (12500000 /* 100 Mbit/sec */),
      burst_duration_microsec     (10000000),
      max_burst_size              (1048576),
      prepush_duration_microsec   (10000000),
      prepush_size                (1048576)
{
}

Uint64
RateLimit::frame (Uint64 const cur_time_microsec,
                  bool   const has_timestamp,
                  Uint64       timestamp_microsec,
                  Uint64 const size)
{
    logD (ratelimit, _this_func,
          "cur_time_microsec ",  cur_time_microsec, ", "
          "has_timestamp ",      has_timestamp, ", "
          "timestamp_microsec ", timestamp_microsec, ", "
          "size ",               size);

    burst_size += size;
    total_size += size;

    if (has_timestamp) {
        if (!got_first_timestamp) {
            first_frame_timestamp_microsec = timestamp_microsec;
            got_first_timestamp = true;
        }

        if (timestamp_microsec > first_frame_timestamp_microsec)
            timestamp_microsec -= first_frame_timestamp_microsec;
        else
            timestamp_microsec = 0;
    } else {
        timestamp_microsec = prv_vts_microsec;
    }

    if (!got_first_frame) {
        got_first_frame = true;
        first_frame_time_microsec = cur_time_microsec;
        burst_start_vts_microsec = timestamp_microsec;
        prv_vts_microsec = timestamp_microsec;
        logD (ratelimit, _this_func, "first frame");
        return 0;
    }

    Uint64 vts_microsec;
    if (has_timestamp
        && timestamp_microsec > prv_vts_microsec
        && timestamp_microsec - prv_vts_microsec >= params.min_frame_duration_microsec)
    {
        vts_microsec = timestamp_microsec;
    } else {
        vts_microsec = prv_vts_microsec + params.min_frame_duration_microsec;
    }
    prv_vts_microsec = vts_microsec;

    if (!prepush_done) {
        if (   vts_microsec - burst_start_vts_microsec < params.prepush_duration_microsec
            && burst_size < params.prepush_size)
        {
            logD (ratelimit, _this_func, "prepush continue");
            return 0;
        }

        prepush_done = true;
        last_prepush_vts_microsec = vts_microsec;
        burst_size = 0;
        logD (ratelimit, _this_func, "prepush done");
        return 0;
    }

    if (!burst_started) {
        burst_start_vts_microsec = vts_microsec;
        burst_started = true;
        logD (ratelimit, _this_func, "burst begin");
    }

    if (   vts_microsec - burst_start_vts_microsec >= params.burst_duration_microsec
        || burst_size >= params.max_burst_size)
    {
        if (logLevelOn (ratelimit, LogLevel::Debug)) {
            if (vts_microsec - burst_start_vts_microsec >= params.burst_duration_microsec) {
                log__ (LogLevel::D, _this_func,
                       "vts_microsec ",             vts_microsec, ", "
                       "burst_start_vts_microsec ", burst_start_vts_microsec, ", "
                       "burst_duration_microsec ",  params.burst_duration_microsec);
            }
            if (burst_size >= params.max_burst_size) {
                log__ (LogLevel::D, _this_func,
                       "burst_size ",     burst_size, ", "
                       "max_burst_size ", params.max_burst_size);
            }
        }

        burst_size = 0;
        burst_started = false;

        Uint64 elapsed_time_microsec = 0;
        if (cur_time_microsec > first_frame_time_microsec)
            elapsed_time_microsec = cur_time_microsec - first_frame_time_microsec;

        Uint64 realtime_wait_microsec = 0;
        if (params.realtime) {
            if (elapsed_time_microsec < vts_microsec - last_prepush_vts_microsec) {
                realtime_wait_microsec = vts_microsec - last_prepush_vts_microsec - elapsed_time_microsec;
                logD (ratelimit, _this_func, "realtime_wait_microsec ", realtime_wait_microsec);
            }
        }

        Uint64 byterate_wait_microsec = 0;
        if (params.bytes_per_second) {
            Uint64 const target_time_microsec = ((total_size * 1000) / params.bytes_per_second) * 1000;
            if (target_time_microsec > cur_time_microsec) {
                byterate_wait_microsec = target_time_microsec - cur_time_microsec;
                logD (ratelimit, _this_func, "byterate_wait_microsec ", byterate_wait_microsec);
            }
        }

        Uint64 wait_microsec = realtime_wait_microsec;
        if (byterate_wait_microsec > wait_microsec)
            wait_microsec = byterate_wait_microsec;

        logD (ratelimit, _this_func, "burst end, wait_microsec ", wait_microsec);
        return wait_microsec;
    }

    logD (ratelimit, _this_func, "burst continue");
    return 0;
}

mt_const void
RateLimit::init (RateLimitParams const &params)
{
    this->params = params;
}

RateLimit::RateLimit ()
    : got_first_frame                (false),
      got_first_timestamp            (false),
      first_frame_timestamp_microsec (0),
      first_frame_time_microsec      (0),
      burst_started                  (false),
      burst_start_vts_microsec       (0),
      burst_size                     (0),
      prepush_done                   (false),
      last_prepush_vts_microsec      (0),
      total_size                     (0),
      prv_vts_microsec               (0)
{}

}

