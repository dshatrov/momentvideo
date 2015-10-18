/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/test_stream_generator.h>


using namespace M;

namespace Moment {

void
TestStreamGenerator::init (PagePool    * const mt_nonnull page_pool,
                           Timers      * const mt_nonnull timers,
                           VideoStream * const mt_nonnull video_stream,
                           Options     * const init_opts)
{
    this->page_pool    = page_pool;
    this->timers       = timers;
    this->video_stream = video_stream;

    if (init_opts)
        opts = *init_opts;

    {
        Byte *frame_buf = NULL;
        if (opts.frame_size > 0) {
            frame_buf = new Byte [opts.frame_size];
            memset (frame_buf, 0, opts.frame_size);
        }

        page_pool->getFillPages (&page_list, ConstMemory (frame_buf, opts.frame_size));

        delete frame_buf;
    }
}

void
TestStreamGenerator::start ()
{
    timers->addTimer_microseconds (
            CbDesc<Timers::TimerCallback> (
                    frameTimerTick,
                    this,
                    this),
            (Time) (opts.frame_duration * 1000 * opts.burst_width),
            true /* periodical */);
}

void
TestStreamGenerator::doFrameTimerTick ()
{
    tick_mutex.lock ();

    for (Uint64 i = 0; i < opts.burst_width; ++i) {
        VideoMessage video_msg;

        if (keyframe_counter == 0) {
            video_msg.frame_type = VideoFrameType::KeyFrame;
            keyframe_counter = opts.keyframe_interval;
        } else {
            video_msg.frame_type = VideoFrameType::InterFrame;
            --keyframe_counter;
        }

        if (first_frame) {
            timestamp_offset = getTimeMilliseconds();
            video_msg.pts_nanosec = opts.start_timestamp * 1000000;
            video_msg.dts_nanosec = video_msg.pts_nanosec;
            first_frame = false;
        } else {
            Time timestamp = getTimeMilliseconds();
            if (timestamp >= timestamp_offset)
                timestamp -= timestamp_offset;
            else
                timestamp = 0;

            timestamp += opts.start_timestamp;

            video_msg.pts_nanosec = timestamp * 1000000;
            video_msg.dts_nanosec = video_msg.pts_nanosec;
        }

        video_msg.codec_id = VideoCodecId::/* Unknown */ SorensonH263;

        PagePool::PageListHead *page_list_ptr = &page_list;
        PagePool::PageListHead tmp_page_list;
        if (!opts.use_same_pages) {
            page_pool->getPages (&tmp_page_list, opts.frame_size);

            {
                PagePool::Page *page = tmp_page_list.first;
                while (page) {
                    memset (page->getData(), (int) page_fill_counter, page->data_len);
                    page = page->getNextMsgPage();
                }
            }

            if (page_fill_counter < 255)
                ++page_fill_counter;
            else
                page_fill_counter = 0;

            page_list_ptr = &tmp_page_list;
        }

        video_msg.page_pool  = page_pool;
        video_msg.page_list  = *page_list_ptr;
        video_msg.msg_len    = opts.frame_size;
        video_msg.msg_offset = 0;

        video_stream->fireVideoMessage (&video_msg);

        if (!opts.use_same_pages)
            page_pool->msgUnref (tmp_page_list.first);
    }

    tick_mutex.unlock ();
}

void
TestStreamGenerator::frameTimerTick (void * const _self)
{
    TestStreamGenerator * const self = static_cast <TestStreamGenerator*> (_self);
    self->doFrameTimerTick ();
}

TestStreamGenerator::TestStreamGenerator (EmbedContainer * const embed_container)
    : Object            (embed_container),
      keyframe_counter  (0),
      first_frame       (true),
      timestamp_offset  (0),
      page_fill_counter (0)
{
}

TestStreamGenerator::~TestStreamGenerator ()
{
    logD_ (_this_func);
}

} // namespace Moment

