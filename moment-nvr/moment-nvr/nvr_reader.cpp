/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/nvr_reader.h>


namespace MomentNvr {

void
NvrReader::reset ()
{
    media_reader = NULL;
    seek_pos_microsec = 0;

    media_desc__audio = NULL;
    got_audio_desc    = false;

    media_desc__video = NULL;
    got_video_desc    = false;

    media_desc_reported = false;
    aac_cdata_reported  = false;
    avc_cdata_reported  = false;

    saved_audio_msg.release ();
    got_saved_audio_msg = false;

    saved_video_msg.release ();
    got_saved_video_msg = false;
}

MediaReader::ReadFrameBackend const NvrReader::read_frame_backend = {
    mediaReader_audioFrame,
    mediaReader_videoFrame
};

MediaReader::ReadFrameResult
NvrReader::mediaReader_audioFrame (AudioMessage * const mt_nonnull audio_msg,
                                   void         * const _self)
{
    NvrReader * const self = static_cast <NvrReader*> (_self);

    logS_ (_self_func "refcnt ", self->getRefCount(), ", audio_msg: ", *audio_msg);

    if (audio_msg->frame_type == AudioFrameType::AacSequenceHeader) {
        if (audio_msg->msg_len == 0)
            return MediaReader::ReadFrameResult_Success;

        if (   self->got_audio_desc
            && self->media_desc__audio
            && self->media_desc__audio->getType() == MediaDesc_Audio::AAC)
        {
            MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (self->media_desc__audio.ptr());

            if (PagePool::pagesEqualToMemory (audio_msg->page_list.first,
                                              audio_msg->msg_offset,
                                              audio_msg->msg_len,
                                              media_desc__aac->getCodecData()))
            {
                return MediaReader::ReadFrameResult_Success;
            }
        }

        PagePool::PageListArray arr (audio_msg->page_list.first, audio_msg->msg_offset, audio_msg->msg_len);

        self->media_desc__audio = grabNew <MediaDesc_AAC> ();
        MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (self->media_desc__audio.ptr());

        media_desc__aac->rate     = audio_msg->rate;
        media_desc__aac->channels = audio_msg->channels;

        media_desc__aac->allocateCodecData (audio_msg->msg_len);
        arr.get (0, media_desc__aac->getCodecData_rw());

        self->got_audio_desc      = true;
        self->aac_cdata_reported  = false;
        self->media_desc_reported = false;

        return MediaReader::ReadFrameResult_Success;
    }

    if (mt_unlikely (self->got_saved_audio_msg)) {
        logD_ (_self_func "frame skipped");
        return MediaReader::ReadFrameResult_BurstLimit;
    }
    self->got_saved_audio_msg = true;

    self->saved_audio_msg.release ();
    self->saved_audio_msg = *audio_msg;
    if (audio_msg->page_pool)
        audio_msg->page_pool->msgRef (audio_msg->page_list.first);

    return MediaReader::ReadFrameResult_BurstLimit;
}

MediaReader::ReadFrameResult
NvrReader::mediaReader_videoFrame (VideoMessage * const mt_nonnull video_msg,
                                   void         * const _self)
{
    NvrReader * const self = static_cast <NvrReader*> (_self);

    logS_ (_self_func "refcnt ", self->getRefCount(), ", video_msg: ", *video_msg);

    if (video_msg->frame_type == VideoFrameType::AvcSequenceHeader) {
        if (video_msg->msg_len == 0)
            return MediaReader::ReadFrameResult_Success;

        if (   self->got_video_desc
            && self->media_desc__video
            && self->media_desc__video->getType() == MediaDesc_Video::AVC)
        {
            MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (self->media_desc__video.ptr());

            if (logLevelOn_ (LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _self_func, "old cdata:");
                logHex_locked__ (LogLevel::Debug, media_desc__avc->getCodecData(), _self_func);
                logUnlock ();
            }

            if (PagePool::pagesEqualToMemory (video_msg->page_list.first,
                                              video_msg->msg_offset,
                                              video_msg->msg_len,
                                              media_desc__avc->getCodecData()))
            {
                return MediaReader::ReadFrameResult_Success;
            }
        }

        if (logLevelOn_ (LogLevel::Debug)) {
            logLock ();
            log_locked__ (LogLevel::Debug, _self_func, "new cdata:");
            PagePool::logPagesMsg_locked (LogLevel::Debug, video_msg->page_list.first, video_msg->msg_offset, video_msg->msg_len, _self_func);
            logUnlock ();
        }

        PagePool::PageListArray arr (video_msg->page_list.first, video_msg->msg_offset, video_msg->msg_len);

        self->media_desc__video = grabNew <MediaDesc_AVC> ();
        MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (self->media_desc__video.ptr());

        media_desc__avc->width     = 640; // TODO Parse AVC cdata to get width/height, get the framerate somehow
        media_desc__avc->height    = 480;
        media_desc__avc->framerate =  25;

        media_desc__avc->allocateCodecData (video_msg->msg_len);
        arr.get (0, media_desc__avc->getCodecData_rw());

        self->got_video_desc      = true;
        self->avc_cdata_reported  = false;
        self->media_desc_reported = false;

        return MediaReader::ReadFrameResult_Success;
    }

    if (mt_unlikely (self->got_saved_video_msg)) {
        logD_ (_self_func "frame skipped");
        return MediaReader::ReadFrameResult_BurstLimit;
    }
    self->got_saved_video_msg = true;

    self->saved_video_msg.release ();
    self->saved_video_msg = *video_msg;
    if (video_msg->page_pool)
        video_msg->page_pool->msgRef (video_msg->page_list.first);

    return MediaReader::ReadFrameResult_BurstLimit;
}

bool
NvrReader::createMediaReaderIfNeeded (ReadResult * const mt_nonnull ret_res)
{
    *ret_res = ReadResult::Normal;

    if (media_reader)
        return false;

    logD_ (_this_func "creating MediaReader");

    media_reader = grabNewObject <MediaReader> ();
    media_reader->init (page_pool,
                        vfs,
                        stream_name,
                        seek_pos_microsec / 1000000,
                        0 /* burst_size_limit */);

    media_reader->setTimelapse (timelapse_frame_interval_millisec,
                                timelapse_frames_per_second);

// Unnecessary
//    // Reading codec data for AAC and AVC.
//    for (unsigned i = 0; i < 2; ++i)
    {
        logD_ (_this_func "calling readMoreData()");
        MediaReader::ReadFrameResult const res = media_reader->readMoreData (&read_frame_backend, this);
        if (res == MediaReader::ReadFrameResult_Failure) {
            logE_ (_this_func, "readMoreData() failed");

            *ret_res = ReadResult::Error;
            return true;
        } else
        if (   res == MediaReader::ReadFrameResult_Finish
            || res == MediaReader::ReadFrameResult_NoData)
        {
            logD_ (_this_func "readMoreData(): no data");

            *ret_res = ReadResult::NoMoreFrames;
            return true;
        }
    }

    return true;
}

bool
NvrReader::reportMediaDescIfNeeded (ReadMessage   * const mt_nonnull read_msg,
                                    MediaMessage ** const mt_nonnull ret_msg,
                                    bool            const report_audio)
{
    if (!media_desc_reported && (got_audio_desc || got_video_desc)) {
        *ret_msg = &read_msg->media_desc_msg;

        if (got_audio_desc)
            read_msg->media_desc_msg.media_desc__audio = media_desc__audio;
        else
            read_msg->media_desc_msg.media_desc__audio = NULL;

        if (got_video_desc)
            read_msg->media_desc_msg.media_desc__video = media_desc__video;
        else
            read_msg->media_desc_msg.media_desc__video = NULL;

        media_desc_reported = true;
        return true;
    }

    if (report_audio) {
        if (   !aac_cdata_reported
            && got_saved_audio_msg
            && got_audio_desc
            && media_desc__audio
            && media_desc__audio->getType() == MediaDesc_Audio::AAC)
        {
            MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (media_desc__audio.ptr());
            AudioMessage * const audio_msg = &read_msg->audio_msg;
            *ret_msg = audio_msg;

            ConstMemory const aac_cdata = media_desc__aac->getCodecData();

            audio_msg->frame_type = AudioFrameType::AacSequenceHeader;
            audio_msg->codec_id   = AudioCodecId::AAC;
            audio_msg->rate       = media_desc__aac->rate;
            audio_msg->channels   = media_desc__aac->channels;

            audio_msg->pts_nanosec = saved_audio_msg.pts_nanosec;
            audio_msg->dts_nanosec = saved_audio_msg.dts_nanosec;

            audio_msg->msg_len    = aac_cdata.len();
            audio_msg->msg_offset = 0;

            audio_msg->page_pool = page_pool;
            page_pool->getFillPages (&audio_msg->page_list, aac_cdata);

            aac_cdata_reported = true;
            return true;
        }
    } else {
        if (   !avc_cdata_reported
            && got_saved_video_msg
            && got_video_desc
            && media_desc__video
            && media_desc__video->getType() == MediaDesc_Video::AVC)
        {
            MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (media_desc__video.ptr());
            VideoMessage * const video_msg = &read_msg->video_msg;
            *ret_msg = video_msg;

            ConstMemory const avc_cdata = media_desc__avc->getCodecData();

            video_msg->frame_type = VideoFrameType::AvcSequenceHeader;
            video_msg->codec_id   = VideoCodecId::AVC;

            video_msg->pts_nanosec = saved_video_msg.pts_nanosec;
            video_msg->dts_nanosec = saved_video_msg.dts_nanosec;

            video_msg->msg_len    = avc_cdata.len();
            video_msg->msg_offset = 0;

            video_msg->page_pool = page_pool;
            page_pool->getFillPages (&video_msg->page_list, avc_cdata);

            avc_cdata_reported = true;
            return true;
        }
    }

    return false;
}

VodReader::ReadResult
NvrReader::readNextFrame (ReadMessage   * const mt_nonnull read_msg,
                          MediaMessage ** const mt_nonnull ret_msg,
                          bool            const stop_time_enabled,
                          Time            const stop_time_microsec)
{
    logS_ (_this_func "stop_time_enabled ", stop_time_enabled, ", stop_time_microsec ", stop_time_microsec);

    if (   !got_saved_audio_msg
        && !got_saved_video_msg)
    {
        ReadResult read_res = ReadResult::Normal;
        if (createMediaReaderIfNeeded (&read_res)) {
            if (read_res != ReadResult::Normal)
                return read_res;
        } else {
            logS_ (_this_func "calling readMoreData()");
            MediaReader::ReadFrameResult const res = media_reader->readMoreData (&read_frame_backend, this);
            if (res == MediaReader::ReadFrameResult_Failure) {
                logE_ (_this_func, "readMoreData() failed");
                return ReadResult::Error;
            } else
            if (   res == MediaReader::ReadFrameResult_Finish
                || res == MediaReader::ReadFrameResult_NoData)
            {
                logD_ (_this_func "readMoreData(): no data");
                return ReadResult::NoMoreFrames;
            }
        }
    }

    if (got_saved_audio_msg
        && (!got_saved_video_msg
            || saved_audio_msg.dts_nanosec <= saved_video_msg.dts_nanosec))
    {
        if (stop_time_enabled && saved_audio_msg.dts_nanosec / 1000 >= stop_time_microsec) {
            logD_ (_this_func "StopTimeReached");
            return ReadResult::StopTimeReached;
        }

        if (reportMediaDescIfNeeded (read_msg, ret_msg, true /* report_audio */)) {
            logD_ (_this_func "media desc reported: ", **ret_msg);
            return ReadResult::Normal;
        }

        read_msg->audio_msg = saved_audio_msg;
        *ret_msg = &read_msg->audio_msg;

        saved_audio_msg.page_pool = NULL;
        saved_audio_msg.page_list.reset ();
        got_saved_audio_msg = false;

        return ReadResult::Normal;
    }

    if (got_saved_video_msg) {
        if (stop_time_enabled && saved_video_msg.dts_nanosec / 1000 >= stop_time_microsec) {
            logD_ (_this_func "StopTimeReached");
            return ReadResult::StopTimeReached;
        }

        if (reportMediaDescIfNeeded (read_msg, ret_msg, false /* report_audio */)) {
            logD_ (_this_func "media desc reported: ", **ret_msg);
            return ReadResult::Normal;
        }

        read_msg->video_msg = saved_video_msg;
        *ret_msg = &read_msg->video_msg;

        saved_video_msg.page_pool = NULL;
        saved_video_msg.page_list.reset ();
        got_saved_video_msg = false;

        return ReadResult::Normal;
    }

    return ReadResult::NoMoreFrames;
}

VodReader::SeekResult
NvrReader::seek (Time const seek_pos_microsec)
{
    logD_ (_this_func, "seek_pos_microsec ", seek_pos_microsec);

    reset ();
    this->seek_pos_microsec = seek_pos_microsec;

    return SeekResult::Normal;
}

Result
NvrReader::getSourceInfo (VodSourceInfo * const mt_nonnull source_info)
{
    source_info->media_desc__audio = NULL;
    source_info->media_desc__video = NULL;

    {
        ReadResult res = ReadResult::Normal;
        if (createMediaReaderIfNeeded (&res)) {
            if (res == ReadResult::Error) {
                logE_ (_this_func, "createMediaReaderIfNeeded() failed");
                return Result::Failure;
            }
        }
    }

    if (got_audio_desc)
        source_info->media_desc__audio = media_desc__audio;

    if (got_video_desc)
        source_info->media_desc__video = media_desc__video;

    return Result::Success;
}

mt_const void
NvrReader::init (PagePool    * const mt_nonnull page_pool,
                 Vfs         * const mt_nonnull vfs,
                 ConstMemory   const stream_name)
{
    this->page_pool   = page_pool;
    this->vfs         = vfs;
    this->stream_name = newString (stream_name);
}

NvrReader::NvrReader ()
    : timelapse_frame_interval_millisec (0),
      timelapse_frames_per_second (0),
      seek_pos_microsec   (0),
      got_audio_desc      (false),
      got_video_desc      (false),
      media_desc_reported (false),
      aac_cdata_reported  (false),
      avc_cdata_reported  (false),
      got_saved_audio_msg (false),
      got_saved_video_msg (false)
{
    logD_ (_this_func_);
}

NvrReader::~NvrReader ()
{
    logD_ (_this_func_);

    reset ();
}

}

