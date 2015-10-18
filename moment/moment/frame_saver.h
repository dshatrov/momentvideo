/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__FRAME_SAVER__H__
#define MOMENT__FRAME_SAVER__H__


#include <moment/media_message.h>


namespace Moment {

mt_unsafe class FrameSaver
{
  private:
    struct SavedVideoFrame
    {
        VideoMessage msg;
    };

    struct SavedAudioFrame
    {
        AudioMessage msg;
    };

    bool const enable;
    bool const send_saved_video_frames;

    bool got_saved_keyframe;
    SavedVideoFrame saved_keyframe;

    List<SavedVideoFrame> saved_interframes;

    bool got_saved_metadata;
    SavedVideoFrame saved_metadata;

    bool got_saved_aac_seq_hdr;
    SavedAudioFrame saved_aac_seq_hdr;

    bool got_saved_avc_seq_hdr;
    SavedVideoFrame saved_avc_seq_hdr;

    Ref<MediaDesc_Audio> media_desc__audio;
    Ref<MediaDesc_Video> media_desc__video;

    List<SavedAudioFrame*> saved_speex_headers;

    void releaseSavedInterframes  ();
    void releaseSavedSpeexHeaders ();

  public:
    void releaseState (bool release_audio = true,
                       bool release_video = true);

    void processAudioFrame (AudioMessage * mt_nonnull msg);

    void processVideoFrame (VideoMessage * mt_nonnull msg);

    void processMediaDesc (MediaDesc_Audio *audio_desc,
                           bool             set_audio,
                           MediaDesc_Video *video_desc,
                           bool             set_video);

#if 0
// Unused
    void copyStateFrom (FrameSaver *frame_saver);
#endif

    struct FrameHandler
    {
        Result (*audioFrame) (AudioMessage * mt_nonnull audio_msg,
                              void         *cb_data);

        Result (*videoFrame) (VideoMessage * mt_nonnull video_msg,
                              void         *cb_data);

        Result (*mediaDesc) (MediaDesc_Audio *audio_desc,
                             bool             set_audio,
                             MediaDesc_Video *video_desc,
                             bool             set_video,
                             void            *cb_data);
    };

    Result reportSavedFrames (FrameHandler const * mt_nonnull frame_handler,
                              void               *cb_data);

    VideoMessage* getAvcSequenceHeader ()
        { return got_saved_avc_seq_hdr ? &saved_avc_seq_hdr.msg : NULL; }

    AudioMessage* getAacSequenceHeader ()
        { return got_saved_aac_seq_hdr ? &saved_aac_seq_hdr.msg : NULL; }

    MediaDesc_Audio* getMediaDesc_Audio () { return media_desc__audio; }
    MediaDesc_Video* getMediaDesc_Video () { return media_desc__video; }

    VideoMessage* getSavedKeyframe () { return got_saved_keyframe ? &saved_keyframe.msg : NULL; }

     FrameSaver (bool enable,
                 bool send_saved_video_frames);

    ~FrameSaver ();
};

}


#endif /* LIBMOMENT__FRAME_SAVER__H__ */

