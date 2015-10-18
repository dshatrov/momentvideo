/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MP4_AV_MUXER__H__
#define MOMENT__MP4_AV_MUXER__H__


#include <libmary/libmary.h>

#include <moment/av_muxer.h>
#include <moment/mp4_muxer.h>


namespace Moment {

using namespace M;

mt_unsafe class Mp4AvMuxer : public AvMuxer
{
private:
    mt_const PagePool *page_pool;

    Mp4Muxer mp4_muxer;

    Size total_frame_size;

    bool   got_first_dts;
    Uint64 first_dts_nanosec;
    Uint64 last_dts_nanosec;

    void doMuxMessage (MediaMessage * mt_nonnull msg);

public:
    mt_throws Result beginMuxing ();
    mt_throws Result endMuxing   ();

    mt_throws Result muxAudioMessage (AudioMessage * mt_nonnull msg);
    mt_throws Result muxVideoMessage (VideoMessage * mt_nonnull msg);

    void flush ();

    mt_const void init (PagePool * const mt_nonnull page_pool,
                        bool       const derive_dts_from_pts)
    {
        this->page_pool = page_pool;
        mp4_muxer.init (page_pool, derive_dts_from_pts);
    }

    Mp4AvMuxer ();
};

}


#endif /* MOMENT__MP4_AV_MUXER__H__ */

