/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/vod_source.h>


namespace Moment {

void
VodSource::setTimelapse (Time   const /* timelapse_frame_interval_millisec */,
                         Uint32 const /* timelapse_frames_per_second */)
{
    logD_ (_this_func, "timelapse is not supported by this kind of VodSource");
}

}

