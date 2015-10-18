/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/camino_reader.h>


namespace MomentNvr {

VodReader::ReadResult
CaminoReader::readNextFrame (ReadMessage   * const mt_nonnull read_msg,
                             MediaMessage ** const mt_nonnull ret_msg,
                             bool            const stop_time_enabled,
                             Time            const stop_time_microsec)
{
    return ReadResult::Error;
}

VodReader::SeekResult
CaminoReader::seek (Time const seek_pos_microsec)
{
    return SeekResult::Error;
}

void
CaminoReader::setTimelapse (Time   const timelapse_frame_interval_millisec,
                            Uint32 const timelapse_frames_per_second)
{
//    this->timelapse_frame_interval_millisec = timelapse_frame_interval_millisec;
//    this->timelapse_frames_per_second       = timelapse_frames_per_second;
}

Result
CaminoReader::getSourceInfo (VodSourceInfo * const mt_nonnull source_info)
{
    return Result::Failure;
}

mt_const void
CaminoReader::init (PagePool    * const mt_nonnull page_pool,
                    Vfs         * const mt_nonnull vfs,
                    ConstMemory   const stream_name)
{
}

}

