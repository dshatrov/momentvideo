/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/frame_saver.h>


namespace Moment {

static LogGroup libMary_logGroup_frame_saver ("frame_saver", LogLevel::I);

void
FrameSaver::processAudioFrame (AudioMessage * const mt_nonnull msg)
{
    logD (frame_saver, _func, "0x", fmt_hex, (UintPtr) this);

    if (!enable)
        return;

    switch (msg->frame_type) {
        case AudioFrameType::AacSequenceHeader: {
            logD (frame_saver, _func, msg->frame_type);

            logD (frame_saver, _func, "AAC SEQUENCE HEADER");

            logS_ (_this_func, "AacSequenceHeader");

            if (got_saved_aac_seq_hdr)
                saved_aac_seq_hdr.msg.page_pool->msgUnref (saved_aac_seq_hdr.msg.page_list.first);

            got_saved_aac_seq_hdr = true;
            saved_aac_seq_hdr.msg = *msg;

            msg->page_pool->msgRef (msg->page_list.first);
        } break;
        case AudioFrameType::SpeexHeader: {
            logD (frame_saver, _func, "SPEEX HEADER");

            if (saved_speex_headers.getNumElements() >= 2) {
                logD (frame_saver, _func, "Wrapping saved speex headers");
                releaseSavedSpeexHeaders ();
            }

            SavedAudioFrame * const frame = new (std::nothrow) SavedAudioFrame;
            assert (frame);
            frame->msg = *msg;
            msg->page_pool->msgRef (msg->page_list.first);

            saved_speex_headers.append (frame);
        } break;
        default:
          // No-op
            ;
    }
}

void
FrameSaver::processVideoFrame (VideoMessage * const mt_nonnull msg)
{
    logD (frame_saver, _func, "0x", fmt_hex, (UintPtr) this, " ", msg->frame_type);

    if (!enable)
        return;

    switch (msg->frame_type) {
        case VideoFrameType::KeyFrame:
        case VideoFrameType::GeneratedKeyFrame: {
            if (got_saved_keyframe)
                saved_keyframe.msg.page_pool->msgUnref (saved_keyframe.msg.page_list.first);

            releaseSavedInterframes ();

            got_saved_keyframe = true;
            saved_keyframe.msg = *msg;

            msg->page_pool->msgRef (msg->page_list.first);
        } break;
        case VideoFrameType::InterFrame:
        case VideoFrameType::DisposableInterFrame: {
            if (!got_saved_keyframe)
                return;

            if (saved_interframes.getNumElements() >= 1000 /* TODO Config parameter for saved frames window. */) {
                logD_ (_func, "Too many interframes to save");
                return;
            }

            saved_interframes.appendEmpty ();
            SavedVideoFrame * const new_frame = &saved_interframes.getLast();
            new_frame->msg = *msg;

            msg->page_pool->msgRef (msg->page_list.first);
        } break;
        case VideoFrameType::AvcSequenceHeader: {
            if (got_saved_avc_seq_hdr)
                saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);

            got_saved_avc_seq_hdr = true;
            saved_avc_seq_hdr.msg = *msg;

            msg->page_pool->msgRef (msg->page_list.first);

//#warning TODO Release saved frames if codec data is different (important for correct video playback)
        } break;
        case VideoFrameType::AvcEndOfSequence: {
            if (got_saved_avc_seq_hdr)
                saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);

            got_saved_avc_seq_hdr = false;
        } break;
        case VideoFrameType::RtmpSetMetaData: {
            if (got_saved_metadata)
                saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);

            got_saved_metadata = true;
            saved_metadata.msg = *msg;

            msg->page_pool->msgRef (msg->page_list.first);
        } break;
        case VideoFrameType::RtmpClearMetaData: {
            if (got_saved_metadata)
                saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);

            got_saved_metadata = false;
        } break;
        default:
          // No-op
            ;
    }
}

void
FrameSaver::processMediaDesc (MediaDesc_Audio * const audio_desc,
                              bool              const set_audio,
                              MediaDesc_Video * const video_desc,
                              bool              const set_video)
{
    logD_ (_this_func,
           "audio_desc 0x", fmt_hex, (UintPtr) audio_desc, " set_audio ", set_audio, " "
           "video_desc 0x", fmt_hex, (UintPtr) video_desc, " set_video ", set_video);

    if (!enable)
        return;

    if (set_audio && audio_desc)
        logD_ (_this_func, "auido_desc: ", *audio_desc);
    if (set_video && video_desc)
        logD_ (_this_func, "video_desc: ", *video_desc);

    if (set_audio)
        media_desc__audio = audio_desc;

    if (set_video)
        media_desc__video = video_desc;

//#warning TODO drop saved frames if media desc is different
}

#if 0
// Unused
void
FrameSaver::copyStateFrom (FrameSaver * const frame_saver)
{
    releaseState ();

    got_saved_keyframe = frame_saver->got_saved_keyframe;
    saved_keyframe = frame_saver->saved_keyframe;
    saved_keyframe.msg.page_pool->msgRef (saved_keyframe.msg.page_list.first);

    got_saved_metadata = frame_saver->got_saved_metadata;
    saved_metadata = frame_saver->saved_metadata;
    saved_metadata.msg.page_pool->msgRef (saved_metadata.msg.page_list.first);

    got_saved_aac_seq_hdr = frame_saver->got_saved_aac_seq_hdr;
    saved_aac_seq_hdr = frame_saver->saved_aac_seq_hdr;
    saved_aac_seq_hdr.msg.page_pool->msgRef (saved_aac_seq_hdr.msg.page_list.first);

    got_saved_avc_seq_hdr = frame_saver->got_saved_avc_seq_hdr;
    saved_avc_seq_hdr = frame_saver->saved_avc_seq_hdr;
    saved_avc_seq_hdr.msg.page_pool->msgRef (saved_avc_seq_hdr.msg.page_list.first);

    {
        saved_interframes.clear ();
        List<SavedVideoFrame>::iter iter (frame_saver->saved_interframes);
        while (!frame_saver->saved_interframes.iter_done (iter)) {
            SavedVideoFrame * const frame = &frame_saver->saved_interframes.iter_next (iter)->data;
            saved_interframes.appendEmpty();
            SavedVideoFrame * const new_frame = &saved_interframes.getLast();
            *new_frame = *frame;
            new_frame->msg.page_pool->msgRef (new_frame->msg.page_list.first);
        }
    }

    {
        saved_speex_headers.clear ();
        List<SavedAudioFrame*>::iter iter (frame_saver->saved_speex_headers);
        while (!frame_saver->saved_speex_headers.iter_done (iter)) {
            SavedAudioFrame * const frame = frame_saver->saved_speex_headers.iter_next (iter)->data;
            SavedAudioFrame * const new_frame = new (std::nothrow) SavedAudioFrame (*frame);
            assert (new_frame);
            new_frame->msg.page_pool->msgRef (new_frame->msg.page_list.first);
            saved_speex_headers.append (new_frame);
        }
    }
}
#endif

Result
FrameSaver::reportSavedFrames (FrameHandler const * const mt_nonnull frame_handler,
                               void               * const cb_data)
{
    if (media_desc__audio || media_desc__video) {
        if (frame_handler->mediaDesc) {
            if (!frame_handler->mediaDesc (media_desc__audio,
                                           media_desc__audio,
                                           media_desc__video,
                                           media_desc__video,
                                           cb_data))
            {
                return Result::Failure;
            }
        }
    }

    if (got_saved_metadata) {
        if (frame_handler->videoFrame) {
            if (!frame_handler->videoFrame (&saved_metadata.msg, cb_data))
                return Result::Failure;
        }
    }

    if (got_saved_aac_seq_hdr) {
        if (frame_handler->audioFrame) {
            if (!frame_handler->audioFrame (&saved_aac_seq_hdr.msg, cb_data))
                return Result::Failure;
        }
    }

    if (got_saved_avc_seq_hdr
        && frame_handler->videoFrame)
    {
        {
            VideoMessage msg;

            msg.pts_nanosec = saved_avc_seq_hdr.msg.pts_nanosec;
            msg.dts_nanosec = saved_avc_seq_hdr.msg.dts_nanosec;
            msg.codec_id = VideoCodecId::AVC;
            msg.frame_type = VideoFrameType::AvcEndOfSequence;

            msg.page_pool = saved_avc_seq_hdr.msg.page_pool;
            msg.msg_offset = 0;

          // TODO Send AvcEndOfSequence only when AvcSequenceHeader was sent.

            if (!frame_handler->videoFrame (&msg, cb_data))
                return Result::Failure;
        }

        if (!frame_handler->videoFrame (&saved_avc_seq_hdr.msg, cb_data))
            return Result::Failure;
    }

    if (frame_handler->audioFrame) {
        List<SavedAudioFrame*>::iter iter (saved_speex_headers);
        while (!saved_speex_headers.iter_done (iter)) {
            SavedAudioFrame * const frame = saved_speex_headers.iter_next (iter)->data;
            if (!frame_handler->audioFrame (&frame->msg, cb_data))
                return Result::Failure;
        }
    }

    {
        if (send_saved_video_frames) {
            if (got_saved_keyframe) {
                if (frame_handler->videoFrame)
                    if (!frame_handler->videoFrame (&saved_keyframe.msg, cb_data))
                        return Result::Failure;
            }

            {
                List<SavedVideoFrame>::iter iter (saved_interframes);
                while (!saved_interframes.iter_done (iter)) {
                    SavedVideoFrame * const frame = &saved_interframes.iter_next (iter)->data;
                    if (frame_handler->videoFrame) {
                       if (!frame_handler->videoFrame (&frame->msg, cb_data))
                           return Result::Failure;
                    }
                }
            }
        }
    }

    return Result::Success;
}

void
FrameSaver::releaseSavedInterframes ()
{
    List<SavedVideoFrame>::iter iter (saved_interframes);
    while (!saved_interframes.iter_done (iter)) {
        SavedVideoFrame * const frame = &saved_interframes.iter_next (iter)->data;
        frame->msg.page_pool->msgUnref (frame->msg.page_list.first);
    }

    saved_interframes.clear ();
}

void
FrameSaver::releaseSavedSpeexHeaders ()
{
    List<SavedAudioFrame*>::iter iter (saved_speex_headers);
    while (!saved_speex_headers.iter_done (iter)) {
        SavedAudioFrame * const frame = saved_speex_headers.iter_next (iter)->data;
        frame->msg.page_pool->msgUnref (frame->msg.page_list.first);
        delete frame;
    }

    saved_speex_headers.clear ();
}

void
FrameSaver::releaseState (bool const release_audio,
                                       bool const release_video)
{
    if (release_video) {
        if (got_saved_keyframe) {
            saved_keyframe.msg.page_pool->msgUnref (saved_keyframe.msg.page_list.first);
            got_saved_keyframe = false;
        }

        if (got_saved_avc_seq_hdr) {
            saved_avc_seq_hdr.msg.page_pool->msgUnref (saved_avc_seq_hdr.msg.page_list.first);
            got_saved_avc_seq_hdr = false;
        }

        releaseSavedInterframes ();
    }

    if (release_audio) {
        if (got_saved_aac_seq_hdr) {
            saved_aac_seq_hdr.msg.page_pool->msgUnref (saved_aac_seq_hdr.msg.page_list.first);
            got_saved_aac_seq_hdr = false;
        }

        releaseSavedSpeexHeaders ();
    }

    // TODO When and how to release this for bound streams?
    if (got_saved_metadata) {
        saved_metadata.msg.page_pool->msgUnref (saved_metadata.msg.page_list.first);
        got_saved_metadata = false;
    }
}

FrameSaver::FrameSaver (bool const enable,
                        bool const send_saved_video_frames)
    : enable                  (enable),
      send_saved_video_frames (send_saved_video_frames),
      got_saved_keyframe      (false),
      got_saved_metadata      (false),
      got_saved_aac_seq_hdr   (false),
      got_saved_avc_seq_hdr   (false)
{
}

FrameSaver::~FrameSaver ()
{
    releaseState ();
}

}

