/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/nvr_recorder.h>


namespace MomentNvr {

NvrRecorder::Recording::Recording ()
    : Object           (embed_container),
      vdat_conn        (this /* embed_container */),
    #ifdef LIBMARY_WIN32_IOCP
      vdat_sender_impl (CbDesc<Overlapped::IoCompleteCallback> ()),
    #endif
      idx_conn         (this /* embed_container */),
    #ifdef LIBMARY_WIN32_IOCP
      idx_sender_impl  (CbDesc<Overlapped::IoCompleteCallback> ()),
    #endif
      cur_vdat_offset  (0)
{
}

Time
NvrRecorder::PrewriteQueueEntry::getDtsNanosec () const
{
    switch (type) {
        case t_Audio: {
            PrewriteQueueEntry_Audio * const audio_entry = static_cast <PrewriteQueueEntry_Audio*> (this);
            return audio_entry->audio_msg.dts_nanosec;
        } break;
        case t_Video: {
            PrewriteQueueEntry_Video * const video_entry = static_cast <PrewriteQueueEntry_Video*> (this);
            return video_entry->video_msg.dts_nanosec;
        } break;
        case t_MediaDesc: {
            PrewriteQueueEntry_MediaDesc * const media_desc_entry = static_cast <PrewriteQueueEntry_MediaDesc*> (this);
            // TODO This doesn't make sense.
            unreachable ();
            return 0;
        } break;
    }

    unreachable ();
    return 0;
}

mt_mutex (mutex) void
NvrRecorder::recordStreamHeaders ()
{
}

mt_mutex (mutex) void
NvrRecorder::recordPrewrite ()
{
}

mt_mutex (mutex) void
NvrRecorder::recordAudioMessage (AudioMessage * const mt_nonnull audio_msg)
{
    if (audio_msg->frame_type == AudioFrameType::AacSequenceHeader) {
        if (got_pending_aac_cdata
            && PagePool::pagesEqualToMemory (audio_msg->page_list.first,
                                             audio_msg->msg_offset,
                                             audio_msg->msg_len,
                                             pending_aac_cdata_buffer.mem))
        {
            return;
        }
    }

    if (audio_msg->frame_type != AudioFrameType::RawData)
        return;

    recordMessage (audio_msg);
}

mt_mutex (mutex) void
NvrRecorder::recordVideoMessage (VideoMessage * const mt_nonnull video_msg)
{
    if (video_msg->frame_type == VideoFrameType::AvcSequenceHeader) {
        if (got_pending_avc_cdata
            && PagePool::pagesEqualToMemory (video_msg->page_list.first,
                                             video_msg->msg_offset,
                                             video_msg->msg_len,
                                             pending_avc_cdata_buffer.mem))
        {
            return;
        }
    }

    if (!video_msg->frame_type.isVideoData())
        return;

    recordMessage (video_msg);
}

mt_mutex (mutex) void
NvrRecorder::putToPrewriteQueue (MediaMessage * const mt_nonnull msg)
{
    PrewriteQueueEntry *prewrite_entry = NULL;
    switch (msg->msg_type) {
        case MediaMessage::t_Audio: {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);

            PrewriteQueueEntry_Audio * const audio_entry = new (std::nothrow) PrewriteQueueEntry_Audio;
            assert (audio_entry);
            audio_entry->audio_msg = *audio_msg;

            prewrite_entry = audio_entry;
        } break;
        case MediaMessage::t_Video: {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);

            PrewriteQueueEntry_Video * const video_entry = new (std::nothrow) PrewriteQueueEntry_Video;
            assert (video_entry);
            video_entry->video_msg = *video_msg;

            prewrite_entry = video_entry;
        } break;
        default:
            unreachable ();
    }

    if (msg->page_pool)
        msg->page_pool->msgRef (msg->page_list.first);

    prewrite_queue.append (prewrite_entry);
    ++prewrite_queue_size;

    while (prewrite_queue_size > prewrite_num_frames_limit
           && prewrite_queue.getFirst())
    {
        prewrite_queue.remove (prewrite_queue.getFirst());
        --prewrite_queue_size;
    }

    for (;;) {
        PrewriteQueueEntry * const first = prewrite_queue.getFirst();
        PrewriteQueueEntry * const last  = prewrite_queue.getLast();
        if (!first || !last)
            return;

        Time const first_dts = first->getDtsNanosec();
        Time const last_dts  = last ->getDtsNanosec();

        if (last_dts >= first_dts
            && last_dts - first_dts > prewrite_nanosec)
        {
            prewrite_queue.remove (first);
            --prewrite_queue_size;
        } else {
            break;
        }
    }
}

mt_mutex (mutex) void
NvrRecorder::recordMessage (MediaMessage * const mt_nonnull msg)
{
#if 0
    switch (msg->msg_type) {
        case MediaMessage::t_Audio: {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);

            if (audio_msg->frame_type == AudioFrameType::AacSequenceHeader) {
                if (got_pending_aac_cdata
                    && PagePool::pagesEqualToMemory (audio_msg->page_list.first,
                                                     audio_msg->msg_offset,
                                                     audio_msg->msg_len,
                                                     pending_aac_cdata_buffer.mem))
                {
                    return;
                }
            }
        } break;
        case MediaMessage::t_Video: {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);

            if (video_msg->frame_type == VideoFrameType::AvcSequenceHeader) {
                if (got_pending_avc_cdata
                    && PagePool::pagesEqualToMemory (video_msg->page_list.first,
                                                     video_msg->msg_offset,
                                                     video_msg->msg_len,
                                                     pending_avc_cdata_buffer.mem))
                {
                    return;
                }
            }
        } break;
        default:
            unreachable ();
    }
#endif

    if (!recording) {
        putToPrewriteQueue (msg);
        return;
    }

    Uint64 unixtime_dts_nanosec;
    if (got_unixtime_offset) {
        unixtime_dts_nanosec = msg->dts_nanosec + unixtime_offset_nanosec;
    } else {
        Time const cur_unixtime_nanosec = getUnixtime * 1000000000;
        unixtime_offset_nanosec = cur_unixtime_nanosec - msg->dts_nanosec;
        got_unixtime_offset = true;

        unixtime_dts_nanosec = cur_unixtime_nanosec;
    }
    prv_unixtime_dts_nanosec = unixtime_dts_nanosec;

    if (postwrite_active) {
        bool end_postwrite = false;

        ++postwrite_frame_counter;
        if (postwrite_frame_counter > postwrite_num_frames_limit) {
            end_postwrite = true;
        } else {
            if (got_postwrite_start_ts) {
                if (unixtime_dts_nanosec >= postwrite_start_ts_nanosec
                    && unixtime_dts_nanosec - postwrite_start_ts_nanosec > postwrite_nanosec)
                {
                    end_postwrite = true;
                }
            } else {
                postwrite_start_ts_nanosec = unixtime_dts_nanosec;
                got_postwrite_start_ts = true;
            }
        }

        if (end_postwrite) {
            logD_ (_this_func, "postwrite end");

            postwrite_active = false;
            recording = NULL;
            return;
        }
    }

    if (unxitime_dts_nanosec >= next_file_unixtime_nanosec) {
        recording = NULL;
        if (!doStartRecording (unixtime_dts_nanosec))
            return;
    }

    switch (msg->msg_type) {
        case MediaMessage::t_Audio: {
            if (got_pending_aac_cdata) {
                // TODO write AacSequenceHeader, write to idx
                got_pending_aac_cdata = false;
            }
        } break;
        case MediaMessage::t_Video: {
            if (got_pending_avc_cdata) {
                // TODO write AvcSequenceHeader, write to idx
                got_pending_avc_cdata = false;
            }
        } break;
        default:
            unreachable ();
    }

    // TODO Actually record the message.
}

mt_mutex (mutex) Result
NvrRecorder::openVdatFile (ConstMemory const _filename,
                           Time        const start_unixtime_nansoec,
                           Uint64      const idx_known_file_size)
{
    StRef<String> const filename = makeString (_filename, ".vdat");

    recording->vdat_file = vfs->openFile (filename->mem(), FileOpenFlags::Create, FileAccessMode::ReadWrite);
    if (!recording->vdat_file) {
        logE_ (_this_func, "openFile() failed for filename \"", filename, "\": ", exc->toString());
        recording = NULL;
        return Result::Failure;
    }

#error TODO sync vdat and idx!
    if (!recording->vdat_file->getFile()->seek (idx_known_file_size, SeekOrigin::Beg)) {
        logE_ (_func, "seek() failed: ", exc->toString());
        recording->vdat_file = NULL;
        recording = NULL;
        return Result::Failure;
    }
}

mt_mutex (mutex) Result
NvrRecorder::openIdxFile (ConstMemory const _filename)
{
}

mt_mutex (mutex) Result
NvrRecorder::doStartRecording (Time const cur_unixtime_nanosec)
{
}

#ifdef LIBMARY_WIN32_IOCP
#endif

mt_sync_domain (writeTask) Result
NvrRecorder::writeMessage (WriteQueueEntry * const mt_nonnull entry)
{
  // TODO
}

mt_unlocks (mutex) void
NvrRecorder::clearWriteQueueAndUnlock ()
{
    WriteQueue tmp_queue;
    tmp_queue.stealAppend (write_queue.getFirst(), write_queue.getLast());

    write_queue.clearNoAction ();
    self->mutex.unlock ();

    // Releasing memory with 'mutex' unlocked.
    tmp_queue.clear ();
}

mt_sync_domain (writeTask) bool
NvrRecorder::writeTask (void * const _self)
{
    NvrRecorder * const self = static_cast <NvrRecorder*> (_self);

    self->mutex.lock ();
    if (mt_unlikely (!self->recording)) {
        logD_ (_self_func, "null recording");
        mt_unlocks (mutex) self->clearWriteQueueAndUnlock ();
        return;
    }

    self->write_task_in_progress = true;

    while (WriteQueueEntry * const entry = write_queue.getFirst()) {
        self->write_queue.removeNoAction (entry);
        self->mutex.unlock ();

        if (mt_unlikely (!self->writeMessage (entry))) {
            logE_ (_self_func, "writeMessage() failed");

            self->mutex.lock ();
            self->write_task_in_progress = false;

            Ref<Recording> tmp_recording = std::move (self->recording);
            mt_unlocks (mutex) self->clearWriteQueueAndUnlock ();

            tmp_recording = NULL;
            delete entry;

            return false /* do not reschedule */;
        }
        delete entry;

        self->mutex.lock ();
    }

    self->write_task_in_progress = false;
    self->mutex.unlock ();

    return false /* do not reschedule */;
}

MediaStream::Events const NvrRecorder::stream_events = {
    stream_audioMessage,
    stream_videoMessage,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
    stream_closed,
    NULL /* numWatchersChanged */,
    stream_mediaDesc
};

void
NvrRecorder::stream_mediaDesc (MediaDesc_Audio * const media_desc__audio,
                               bool              const set_audio,
                               MediaDesc_Video * const media_desc__video,
                               bool              const set_video,
                               void            * const _stream_ticket)
{
    if (set_audio && media_desc__audio) {
        if (media_desc__audio->getType() == MediaDesc_Audio::AAC) {
            MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (media_desc__audio);

            ConstMemory const cdata_mem = media_desc__aac->getCodecData();
            if (!got_pending_aac_cdata
                || !equal (pending_aac_cdata_buffer.mem, cdata_mem))
            {
                pending_aac_cdata_buffer.allocate (cdata_mem.len());
                memcpy (pending_aac_cdata_buffer.mem.buf(), cdata_mem.buf(), cdata_mem.len());
                got_pending_aac_cdata = true;
            }
        }
    }

    if (set_video && media_desc__video) {
        if (media_desc__video->getType() == MediaDesc_Video::AVC) {
            MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (media_desc__video);

            ConstMemory const cdata_mem = media_desc__avc->getCodecData();
            if (!got_pending_avc_cdata
                || !equal (pending_avc_cdata_buffer.mem, cdata_mem))
            {
                pending_avc_cdata_buffer.allocate (cdata_mem.len());
                memcpy (pending_avc_cdata_buffer.mem.buf(), cdata_mem.buf(), cdata_mem.len());
                got_pending_avc_cdata = true;
            }
        }
    }
}

void
NvrRecorder::stream_audioMessage (AudioMessage * const mt_nonnull audio_msg,
                                  void         * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    NvrRecorder * const self = stream_ticket->nvr_recorder;

    self->mutex.unlock ();
    if (stream_ticket != self->cur_stream_ticket) {
        self->mutex.unlock ();
        return;
    }

    self->recordAudioMessage (audio_msg);
    self->mutex.unlock ();
}

void
NvrRecorder::stream_videoMessage (VideoMessage * const mt_nonnull video_msg,
                                  void         * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    NvrRecorder * const self = stream_ticket->nvr_recorder;

    self->mutex.lock ();
    if (stream_ticket != self->cur_stream_ticket) {
        self->mutex.unlock ();
        return;
    }

    self->recordVideoMessage (video_msg);
    self->mutex.unlock ();
}

void
NvrRecorder::stream_closed (void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    NvrRecorder * const self = stream_ticket->nvr_recorder;

    self->mutex.lock ();
    if (stream_ticket != self->cur_stream_ticket) {
        self->mutex.unlock ();
        return;
    }

    if (self->cur_stream)
        self->cur_stream->getEventInformer()->unsubscribe (self->stream_sbn);

    self->cur_stream_ticket = NULL;
    self->cur_stream = NULL;
    self->stream_sbn = NULL;

    self->recording = NULL;

    self->mutex.unlock ();
}

void
NvrRecorder::setMediaStream (MediaStream * const stream)
{
    Ref<StreamTicket> const new_ticket = grab (new (std::nothrow) StreamTicket (this));

    mutex.lock ();

    if (cur_stream == stream) {
        logD_ (_this_func, "same stream");
        mutex.unlock ();
        return;
    }

    got_unixtime_offset = false;

    if (cur_stream) {
        cur_stream->getEventInformer()->unsubscribe (stream_sbn);
        stream_sbn = NULL;
    }

    if (stream) {
        stream_sbn = stream->getEventInformer()->subscribe (
                CbDesc<MediaStream::Events> (&stream_events, new_ticket, this, new_ticket));
    }

    cur_stream_ticket = new_ticket;
    cur_stream = stream;

    recordStreamHeaders ();

    mutex.unlock ();
}

void
NvrRecorder::startRecording ()
{
    mutex.lock ();
    doStartRecording (getUnixtime() * 1000000000);
    mutex.unlock ();
}

void
NvrRecorder::stopRecording ()
{
    mutex.lock ();

    if (postwrite_nanosec == 0 || postwrite_num_frames_limit == 0) {
        recording = NULL;
    } else
    if (!postwite_active) {
        logD_ (_this_func, "postwrite begin");

        postwrite_active = true;
        postwrite_frame_counter = 0;

        if (got_unixtime_offset) {
            got_postwrite_start_dts = true;
            postwrite_start_dts_nanosec = prv_unixtime_dts_nanosec;
        } else {
            got_postwrite_start_dts = false;
            postwrite_start_dts_nanosec = 0;
        }
    }

    mutex.unlock ();
}

bool
NvrRecorder::isRecording ()
{
    mutex.lock ();
    bool const res = (bool) recording && !postwrite_active;
    mutex.unlock ();
    return res;
}

void
NvrRecorder::init (PagePool            * const mt_nonnull page_pool,
                   ServerThreadContext * const mt_nonnull thread_ctx,
                   Vfs                 * const mt_nonnull vfs,
                   ConstMemory           const channel_name,
                   Time                  const prewrite_microsec,
                   Count                 const prewrite_num_frames_limit,
                   Time                  const postwrite_microsec,
                   Count                 const postwrite_num_frames_limit)
{
    this->page_pool  = page_pool;
    this->thread_ctx = thread_ctx;
    this->vfs        = vfs;

    this->channel_name = newString (channel_name);

    this->prewrite_microsec          = prewrite_microsec;
    this->prewrite_num_frames_limit  = prewrite_num_frames_limit;
    this->postwrite_microsec         = postwrite_microsec;
    this->postwrite_num_frames_limit = postwrite_num_frames_limit;

    deferred_reg.setDeferredProcessor (thread_ctx->getDeferredProcessor());
}

NvrRecorder::NvrRecorder (EmbedContainer * const embed_container)
    : Object                         (embed_container),

      write_queue_max_total_pagesize (0),
      write_queue_max_num_entries    (0),

      prewrite_microsec              (0),
      prewrite_num_frames_limit      (0)
      postwrite_microsec             (0),
      postwrite_num_frames_limit     (0),

      got_unixtime_offset            (false),
      unixtime_offset_nanosec        (0),
      prv_unixtime_dts_nanosec       (0),

      got_aac_cdata                  (false),
      got_pending_aac_cdata          (false),

      got_avc_cdata                  (false),
      got_pending_avc_cdata          (false),

      next_file_unixtime_nanosec     (0),

      write_queue_total_pagesize     (0),
      write_queue_num_entries        (0),
      write_task_in_progress         (false),

      postwrite_active               (false),
      got_postwrite_start_dts        (false),
      postwrite_start_dts_nanosec    (0),
      postwrite_frame_counter        (0),

      prewrite_queue_size            (0)
{
    write_task.cb = CbDesc<DeferredProcessor::TaskCallback> (writeTask, this, this);
}

NvrRecorder::~NvrRecorder ()
{
#if 0
// Unnecessary

    if (got_pending_aac_cdata) {
        pending_aac_cdata_pages.release ();
        got_pending_aac_cdata = false;
    }

    if (got_pending_avc_cdata) {
        pending_avc_cdata_pages.release ();
        got_pending_avc_cdata = false;
    }
#endif
}

}

