/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__RATE_LIMIT__H__
#define MOMENT__RATE_LIMIT__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

struct RateLimitParams
{
    bool   realtime;
    Uint64 min_frame_duration_microsec;
    Uint64 bytes_per_second;
    Uint64 burst_duration_microsec;
    Uint64 max_burst_size;
    Uint64 prepush_duration_microsec;
    Uint64 prepush_size;

    void dump (LogLevel    loglevel,
               ConstMemory prefix);

    RateLimitParams ();
};

mt_unsafe class RateLimit
{
  private:
    mt_const RateLimitParams params;

    bool   got_first_frame;
    bool   got_first_timestamp;
    Uint64 first_frame_timestamp_microsec;
    Uint64 first_frame_time_microsec;

    bool   burst_started;
    Uint64 burst_start_vts_microsec;
    Uint64 burst_size;

    bool   prepush_done;
    Uint64 last_prepush_vts_microsec;

    Uint64 total_size;

    // "Virtual timestamp" is determined by ensuring that every frame's virtual
    // timestamp is ahead of the previous one by at least min_frame_duration_microsec.
    Uint64 prv_vts_microsec;

  public:
    Uint64 frame (Uint64 cur_time_microsec,
                  bool   has_timestamp,
                  Uint64 timestamp_microsec,
                  Uint64 size);

    mt_const void init (RateLimitParams const &params);

    RateLimit ();
};

}


#endif /* MOMENT__RATE_LIMIT__H__ */

