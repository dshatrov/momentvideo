/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__TEST_STREAM_GENERATOR__H__
#define MOMENT__TEST_STREAM_GENERATOR__H__


#include <libmary/libmary.h>

#include <moment/stream.h>


namespace Moment {

using namespace M;

class TestStreamGenerator : public Object
{
  private:
    Mutex tick_mutex;

  public:
    struct Options
    {
        Uint64 frame_duration    = 40;
        Uint64 frame_size        = 2500;
        Uint64 keyframe_interval = 10;
        Uint64 start_timestamp   = 0;
        Uint64 burst_width       = 1;
        bool use_same_pages      = true;
    };

  private:
    mt_const Options opts;

    mt_const Ref<PagePool>    page_pool;
    mt_const Ref<Timers>      timers;
    mt_const Ref<VideoStream> video_stream;

    mt_mutex (tick_mutex) Uint64 keyframe_counter;

    mt_mutex (tick_mutex) PagePool::PageListHead page_list;

    mt_mutex (tick_mutex) bool first_frame;
    mt_mutex (tick_mutex) Time timestamp_offset;

    mt_mutex (tick_mutex) Uint32 page_fill_counter;

    void doFrameTimerTick ();

    static void frameTimerTick (void *_self);

  public:
    void init (PagePool    * mt_nonnull page_pool,
               Timers      * mt_nonnull timers,
               VideoStream * mt_nonnull video_stream,
               Options  *opts);

    void start ();

     TestStreamGenerator (EmbedContainer *embed_container);
    ~TestStreamGenerator ();
};

}


#endif /* MOMENT__TEST_STREAM_GENERATOR__H__ */

