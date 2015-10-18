/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__VOD_READER__H__
#define MOMENT__VOD_READER__H__


#include <moment/vod_source.h>


namespace Moment {

mt_unsafe class VodReader : public StReferenced
{
  public:
    enum_beg (ReadResult)
        enum_values (Normal, Error, NoMoreFrames, StopTimeReached)
    enum_end (ReadResult)

    enum_beg (SeekResult)
        enum_values (Normal, Error, SeekPastEnd)
    enum_end (SeekResult)

    struct ReadMessage
    {
        AudioMessage     audio_msg;
        VideoMessage     video_msg;
        MediaDescMessage media_desc_msg;
    };

    virtual ReadResult readNextFrame (ReadMessage   * mt_nonnull read_msg,
                                      MediaMessage ** mt_nonnull ret_msg,
                                      bool           stop_time_enabled,
                                      Time           stop_time_microsec) = 0;

    virtual SeekResult seek (Time seek_pos_microsec) = 0;

    // 0 means no timelapse.
    virtual void setTimelapse (Time   timelapse_frame_interval_millisec,
                               Uint32 timelapse_frames_per_second);

    virtual Result getSourceInfo (VodSourceInfo * mt_nonnull source_info) = 0;
};

}


#endif /* MOMENT__VOD_READER__H__ */

