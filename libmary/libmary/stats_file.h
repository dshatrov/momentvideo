/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__STATS_FILE__
#define LIBMARY__STATS_FILE__


#include <libmary/native_file.h>


namespace M {

class StatsFile
{
private:
    mt_const Uint64 *value_buf;
    mt_const Size    value_buf_size;

    mt_const Uint64 total_values;
    mt_const Uint64 time_step;

    mt_mutex (mutex) Time buf_time;
    mt_mutex (mutex) Size value_buf_pos;

    Result doWriteValues (Time    time,
                          Uint64 * mt_nonnull values,
                          Count   num_values);

public:
    Result writeValue (Time   time,
                       Uint64 value);

    Result readValues (Time    time,
                       Uint64 *values,
                       Count   num_values,
                       Count  *ret_num_got);

    mt_const Result open (ConstMemory filename);

    mt_const void init (Size   value_buf_size,
                        Uint64 total_values,
                        Uint64 time_step);

     StatsFile ();
    ~StatsFile ();
};

}


#endif /* LIBMARY__STATS_FILE__ */

