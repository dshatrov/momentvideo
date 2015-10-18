/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__VOD_SOURCE__H__
#define MOMENT__VOD_SOURCE__H__


#include <moment/media_source.h>
#include <moment/media_message.h>


namespace Moment {

struct VodSourceInfo
{
    Ref<MediaDesc_Audio> media_desc__audio;
    Ref<MediaDesc_Video> media_desc__video;
};

class VodSource : public MediaSource
{
  public:
    enum_beg (EndReason)
        enum_values (NoMoreFrames,
                     StopTimeReached,
                     Stopped,
                     Error)
    enum_end (EndReason)

    struct Frontend
    {
        // TODO Return 'true' to block (replaces setAutoBlock()).
        void (*mediaMessage) (MediaMessage * mt_nonnull msg,
                              void         *cb_data);

        void (*endOfSource) (EndReason  end_reason,
                             void      *cb_data);
    };

    virtual Timers* getTimers () = 0;

    // Should be called before the next asynchronous operation (start/querySourceInfo),
    // when no other async operations are in progress.
    virtual void setFrontend (CbDesc<VodSource::Frontend> const &frontend) = 0;

    virtual void start  (bool stop_time_enabled,
                         Time stop_time_microsec) = 0;
    virtual void stop   () = 0;

    virtual void block  () = 0;
    virtual void resume () = 0;

    // TODO seek flags: rounding direction (seek_before/after), seek_to_keyframe.
    virtual void seek (Time seek_pos_microsec) = 0;

    virtual void setTimelapse (Time   timelapse_frame_interval_millisec,
                               Uint32 timelapse_frames_per_second);

    typedef void (QuerySourceInfoCallback) (VodSourceInfo *source_info,
                                            void          *cb_data);

    // Only one querySourceInfo() operation at a time is allowed.
    virtual bool querySourceInfo (CbDesc<VodSource::QuerySourceInfoCallback> const &cb,
                                  VodSourceInfo * mt_nonnull ret_source_info) = 0;

    VodSource (EmbedContainer * const embed_container)
        : MediaSource (embed_container, MediaSource::Type::VodSource)
    {}
};

}


#endif /* MOMENT__VOD_SOURCE__H__ */

