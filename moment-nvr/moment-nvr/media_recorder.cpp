/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/media_recorder.h>


namespace MomentNvr {

mt_mutex (mutex) void
MediaRecorder::recordStreamHeaders ()
{
    if (!cur_stream)
        return;

    cur_stream->lock ();
    FrameSaver * const frame_saver = cur_stream->getFrameSaver();

//#warning TODO write MediaDesc_Audio and MediaDesc_Video

    if (AudioMessage * const audio_msg = frame_saver->getAacSequenceHeader()) {
        if (got_pending_aac_seq_hdr)
            pending_aac_seq_hdr.release ();

        pending_aac_seq_hdr = *audio_msg;
        if (pending_aac_seq_hdr.page_pool)
            pending_aac_seq_hdr.page_pool->msgRef (pending_aac_seq_hdr.page_list.first);

        got_pending_aac_seq_hdr = true;
    }

    if (VideoMessage * const video_msg = frame_saver->getAvcSequenceHeader()) {
        if (got_pending_avc_seq_hdr)
            pending_avc_seq_hdr.release ();

        pending_avc_seq_hdr = *video_msg;
        if (pending_avc_seq_hdr.page_pool)
            pending_avc_seq_hdr.page_pool->msgRef (pending_avc_seq_hdr.page_list.first);

        got_pending_avc_seq_hdr = true;
        got_avc_seq_hdr = true;
    }

    cur_stream->unlock ();
}

mt_mutex (mutex) void
MediaRecorder::recordPrewrite ()
{
    PrewriteQueue::iterator iter (prewrite_queue);
    while (!iter.done()) {
        PrewriteQueueEntry * const prewrite_entry = iter.next ();
        switch (prewrite_entry->entry_type) {
            case PrewriteQueueEntry::EntryType_Audio:
                logD_ (_func, "prewrite frame ts ", prewrite_entry->audio_msg.dts_nanosec, " ",
                       prewrite_entry->audio_msg.frame_type);
                recordAudioMessage (&prewrite_entry->audio_msg);
                break;
            case PrewriteQueueEntry::EntryType_Video:
                logD_ (_func, "prewrite frame ts ", prewrite_entry->video_msg.dts_nanosec, " ",
                       prewrite_entry->video_msg.frame_type);
                recordVideoMessage (&prewrite_entry->video_msg);
                break;
        }
    }
    prewrite_queue.clear ();
}

mt_mutex (mutex) void
MediaRecorder::recordAudioMessage (AudioMessage * const mt_nonnull audio_msg)
{
    Byte header [2];
    header [0] = (Byte) 2 /* audio_message */;
    header [1] = (Byte) toAudioRecordFrameType (audio_msg->frame_type);

    recordMessage (audio_msg, true /* is_audio_msg */, ConstMemory::forObject (header));
}

mt_mutex (mutex) void
MediaRecorder::recordVideoMessage (VideoMessage * const mt_nonnull video_msg)
{
    if (!got_avc_seq_hdr) {
        if (video_msg->frame_type == VideoFrameType::AvcSequenceHeader) {
            got_avc_seq_hdr = true;
        } else {
            logD_ (_this_func, "no AVC sequence header, skipping video messsage");
            return;
        }
    }

    Byte header [2];
    header [0] = (Byte) 1 /* video message */;
    header [1] = (Byte) toVideoRecordFrameType (video_msg->frame_type);

    recordMessage (video_msg, false /* is_audio_msg */, ConstMemory::forObject (header));
}

mt_mutex (mutex) void
MediaRecorder::recordMessage (MediaMessage * const mt_nonnull msg,
                              bool           const is_audio_msg,
                              ConstMemory    const header)
{
    logS_ (_func, "ts ", msg->dts_nanosec);

    if (!recording) {
        logS_ (_func, "not recording");

        PrewriteQueueEntry * const prewrite_entry = new (std::nothrow) PrewriteQueueEntry;
        assert (prewrite_entry);

        if (is_audio_msg) {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);
            prewrite_entry->audio_msg = *audio_msg;
            prewrite_entry->entry_type = PrewriteQueueEntry::EntryType_Audio;
        } else {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
            prewrite_entry->video_msg = *video_msg;
            prewrite_entry->entry_type = PrewriteQueueEntry::EntryType_Video;
        }

        if (msg->page_pool)
            msg->page_pool->msgRef (msg->page_list.first);

        prewrite_queue.append (prewrite_entry);
        ++prewrite_queue_size;

        for (;;) {
            PrewriteQueueEntry * const first = prewrite_queue.getFirst();
            PrewriteQueueEntry * const last  = prewrite_queue.getLast();

            if (!first || !last)
                break;

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

        while (prewrite_queue_size > prewrite_num_frames_limit
               && prewrite_queue.getFirst())
        {
            prewrite_queue.remove (prewrite_queue.getFirst());
            --prewrite_queue_size;
        }

        return;
    }

    Uint64 unixtime_dts_nanosec = prv_unixtime_dts_nanosec;
    if (VideoStream::msgHasTimestamp (msg, is_audio_msg)) {
        if (got_unixtime_offset) {
            unixtime_dts_nanosec = msg->dts_nanosec + unixtime_offset_nanosec;
        } else {
            Time const cur_unixtime_nanosec = getUnixtime() * 1000000000;
            unixtime_offset_nanosec = cur_unixtime_nanosec - msg->dts_nanosec;
            got_unixtime_offset = true;

            unixtime_dts_nanosec = cur_unixtime_nanosec;
        }

        prv_unixtime_dts_nanosec = unixtime_dts_nanosec;
    }

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
            logD_ (_func, "postwrite end");

            postwrite_active = false;
            recording = NULL;
            return;
        }
    }

    logS_ (_func, "ts: ", unixtime_dts_nanosec, ", next: ", next_file_unixtime_nanosec);
    if (unixtime_dts_nanosec >= next_file_unixtime_nanosec) {
        recording = NULL;
        if (!doStartRecording (unixtime_dts_nanosec))
            return;
    }

    if (got_pending_aac_seq_hdr) {
        Byte header [2];
        header [0] = (Byte) 2 /* audio_message */;
        header [1] = (Byte) AudioRecordFrameType::AacSequenceHeader;

        logS_ (_func, "recording AacSequenceHeader");
        doRecordMessage (&pending_aac_seq_hdr, ConstMemory::forObject (header), unixtime_dts_nanosec);

        pending_aac_seq_hdr.release ();
        got_pending_aac_seq_hdr = false;
    }

    if (got_pending_avc_seq_hdr) {
        Byte header [2];
        header [0] = (Byte) 1 /* video message */;
        header [1] = (Byte) VideoRecordFrameType::AvcSequenceHeader;

        logS_ (_func, "recording AvcSequenceHeader");
        doRecordMessage (&pending_avc_seq_hdr, ConstMemory::forObject (header), unixtime_dts_nanosec);

        pending_avc_seq_hdr.release ();
        got_pending_avc_seq_hdr = false;
    }

    doRecordMessage (msg, header, unixtime_dts_nanosec);
}

mt_mutex (mutex) void
MediaRecorder::doRecordMessage (MediaMessage * const mt_nonnull msg,
                                ConstMemory    const header,
                                Time           const unixtime_timestamp_nanosec)
{
    if (unixtime_timestamp_nanosec >= next_idx_unixtime_nanosec) {
        next_idx_unixtime_nanosec = unixtime_timestamp_nanosec + 5000000000;

        Byte idx_entry [16];

        idx_entry [0] = (unixtime_timestamp_nanosec >> 56) & 0xff;
        idx_entry [1] = (unixtime_timestamp_nanosec >> 48) & 0xff;
        idx_entry [2] = (unixtime_timestamp_nanosec >> 40) & 0xff;
        idx_entry [3] = (unixtime_timestamp_nanosec >> 32) & 0xff;
        idx_entry [4] = (unixtime_timestamp_nanosec >> 24) & 0xff;
        idx_entry [5] = (unixtime_timestamp_nanosec >> 16) & 0xff;
        idx_entry [6] = (unixtime_timestamp_nanosec >>  8) & 0xff;
        idx_entry [7] = (unixtime_timestamp_nanosec >>  0) & 0xff;

        idx_entry [ 8] = (recording->cur_data_offset >> 56) & 0xff;
        idx_entry [ 9] = (recording->cur_data_offset >> 48) & 0xff;
        idx_entry [10] = (recording->cur_data_offset >> 40) & 0xff;
        idx_entry [11] = (recording->cur_data_offset >> 32) & 0xff;
        idx_entry [12] = (recording->cur_data_offset >> 24) & 0xff;
        idx_entry [13] = (recording->cur_data_offset >> 16) & 0xff;
        idx_entry [14] = (recording->cur_data_offset >>  8) & 0xff;
        idx_entry [15] = (recording->cur_data_offset >>  0) & 0xff;

        SenderMessageEntry_Pages * const idx_msg = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
        idx_msg->init (/*first_page=*/ NULL, page_pool, /*msg_offs=*/ 0, /*msg_len=*/ 0);
        Byte * const msg_header = idx_msg->getHeaderData();
        memcpy (msg_header, idx_entry, sizeof (idx_entry));
        idx_msg->header_len = sizeof (idx_entry);
        recording->idx_sender->sendMessage (idx_msg, true /* do_flush */);
    }

    SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
    msg_pages->init (msg->page_list.first, msg->page_pool, msg->msg_offset, msg->msg_len);
    msg->page_pool->msgRef (msg->page_list.first);

    {
        Byte * const msg_header = msg_pages->getHeaderData();
        msg_header [0] = (unixtime_timestamp_nanosec >> 56) & 0xff;
        msg_header [1] = (unixtime_timestamp_nanosec >> 48) & 0xff;
        msg_header [2] = (unixtime_timestamp_nanosec >> 40) & 0xff;
        msg_header [3] = (unixtime_timestamp_nanosec >> 32) & 0xff;
        msg_header [4] = (unixtime_timestamp_nanosec >> 24) & 0xff;
        msg_header [5] = (unixtime_timestamp_nanosec >> 16) & 0xff;
        msg_header [6] = (unixtime_timestamp_nanosec >>  8) & 0xff;
        msg_header [7] = (unixtime_timestamp_nanosec >>  0) & 0xff;

        Size const len = msg->msg_len + header.len();
        msg_header [ 8] = (len >> 24) & 0xff;
        msg_header [ 9] = (len >> 16) & 0xff;
        msg_header [10] = (len >>  8) & 0xff;
        msg_header [11] = (len >>  0) & 0xff;

        memcpy (msg_header + 12, header.mem(), header.len());
        msg_pages->header_len = 12 + header.len();

        recording->cur_data_offset += 12 + header.len() + msg->msg_len;
    }

    recording->vdat_sender->sendMessage (msg_pages, true /* do_flush */);
}

mt_mutex (mutex) Result
MediaRecorder::openVdatFile (ConstMemory const _filename,
                             Time        const start_unixtime_nanosec)
{
    StRef<String> const filename = makeString (_filename, ".vdat");

    recording->vdat_file = vfs->openFile (filename->mem(), FileOpenFlags::Create, FileAccessMode::ReadWrite);
    if (!recording->vdat_file) {
        logE_ (_func, "vfs->openFile() failed for filename ",
               filename, ": ", exc->toString());
        recording = NULL;
        return Result::Failure;
    }

    if (!recording->vdat_file->getFile()->seek (0, SeekOrigin::End)) {
        logE_ (_func, "seek() failed: ", exc->toString());
        recording->vdat_file = NULL;
        recording = NULL;
        return Result::Failure;
    }

    recording->vdat_conn->init (thread_ctx->getDeferredProcessor(), recording->vdat_file->getFile());

    recording->vdat_sender->setConnection (recording->vdat_conn);
    recording->vdat_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    recording->vdat_sender->setFrontend (
            CbDesc<Sender::Frontend> (&sender_frontend, recording, recording));

    {
        FileSize file_size = 0;
        if (!recording->vdat_file->getFile()->tell (&file_size)) {
            logE_ (_func, "tell() failed: ", exc->toString());
            recording->vdat_file = NULL;
            recording = NULL;
            return Result::Failure;
        }

        if (file_size == 0) {
            Byte const header [] = { 'M', 'M', 'N', 'T',
                                     // Format version
                                     0, 0, 0, 1,
                                     (Byte) ((start_unixtime_nanosec >> 56) & 0xff),
                                     (Byte) ((start_unixtime_nanosec >> 48) & 0xff),
                                     (Byte) ((start_unixtime_nanosec >> 40) & 0xff),
                                     (Byte) ((start_unixtime_nanosec >> 32) & 0xff),
                                     (Byte) ((start_unixtime_nanosec >> 24) & 0xff),
                                     (Byte) ((start_unixtime_nanosec >> 16) & 0xff),
                                     (Byte) ((start_unixtime_nanosec >>  8) & 0xff),
                                     (Byte) ((start_unixtime_nanosec >>  0) & 0xff),
                                     // Header data length
                                     0, 0, 0, 0 };

            SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);

            PagePool::PageListInfo page_list;
            page_pool->getFillPages (&page_list, ConstMemory::forObject (header));

            msg_pages->init (page_list.first, page_pool, /*msg_offs=*/ 0, page_list.data_len);

            recording->vdat_sender->sendMessage (msg_pages, true /* do_flush */);
        } else {
            Size const file_header_len = 20 /* TODO Fixed file header length */;
            if (file_size > file_header_len) {
                recording->cur_data_offset = file_size - 20;
            } else {
              // TODO Incomplete header - rewrite.
            }
        }
    }

    return Result::Success;
}

mt_mutex (mutex) Result
MediaRecorder::openIdxFile (ConstMemory const _filename)
{
    StRef<String> const filename = makeString (_filename, ".idx");

    recording->idx_file = vfs->openFile (filename->mem(), FileOpenFlags::Create, FileAccessMode::ReadWrite);
    if (!recording->idx_file) {
        logE_ (_func, "vfs->openFile() failed for filename ",
               filename, ": ", exc->toString());
        recording = NULL;
        return Result::Failure;
    }

    if (!recording->idx_file->getFile()->seek (0, SeekOrigin::End)) {
        logE_ (_func, "seek() failed: ", exc->toString());
        recording->idx_file = NULL;
        recording = NULL;
        return Result::Failure;
    }

    // TODO Do tell() and then seek to a multiple of idx entry size (16).
    // TODO After that, get the last idx entry and find the last valid frame
    //      in vdat, discarding trailing garbage.

    recording->idx_conn->init (thread_ctx->getDeferredProcessor(), recording->idx_file->getFile());

    recording->idx_sender->setConnection (recording->idx_conn);
    recording->idx_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    recording->idx_sender->setFrontend (
            CbDesc<Sender::Frontend> (&sender_frontend, recording, recording));

    return Result::Success;
}

mt_mutex (mutex) Result
MediaRecorder::doStartRecording (Time const cur_unixtime_nanosec)
{
//    logD_ (_func, "cur_unixtime_nanosec: ", cur_unixtime_nanosec);

    postwrite_active = false;

    if (recording) {
        logW_ (_func, "already recording");
        return Result::Failure;
    }

    next_idx_unixtime_nanosec = 0;

    recording = grabNewObject <Recording> ();
    recording->weak_media_recorder = this;

    Time next_unixtime_sec;
    StRef<String> const filename =
            naming_scheme->getPath (channel_name->mem(),
                                    cur_unixtime_nanosec / 1000000000,
                                    &next_unixtime_sec);
    if (!filename) {
        logE_ (_func, "naming_scheme->getPath() failed");
        recording = NULL;
        return Result::Failure;
    }
//    logD_ (_func, "filename: ", filename, ", "
//           "next_unixtime_sec: ", next_unixtime_sec, ", cur unixtime: ", cur_unixtime_nanosec);
    next_file_unixtime_nanosec = next_unixtime_sec * 1000000000;

    if (!vfs->createSubdirsForFilename (filename->mem())) {
        logE_ (_func, "vfs->createSubdirsForFilename() failed, filename: ",
               filename->mem(), ": ", exc->toString());
        recording = NULL;
        return Result::Failure;
    }

    if (!openVdatFile (filename->mem(), cur_unixtime_nanosec))
        goto _failure;

    if (!openIdxFile (filename->mem()))
        goto _failure;

    recordStreamHeaders ();
    recordPrewrite ();

    return Result::Success;

  _failure:
    recording->vdat_file = NULL;
    recording->idx_file = NULL;
    recording = NULL;
    return Result::Failure;
}

Sender::Frontend const MediaRecorder::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
MediaRecorder::senderStateChanged (SenderState   const /* sender_state */,
                                   void        * const /* _recording */)
{
  // TODO Start dropping frames when destination is overloaded.
  //      This doesn't matter for local files, because local write operations
  //      always block till completion.
}

void
MediaRecorder::senderClosed (Exception * const exc_,
                             void      * const _recording)
{
    Recording * const recording = static_cast <Recording*> (_recording);

    if (exc_)
        logE_ (_func, "exception: ", exc_->toString());

    Ref<MediaRecorder> const self = recording->weak_media_recorder.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    // TODO Clarify when we hit this path.
    recording->vdat_file = NULL;
    recording->idx_file = NULL;
    // TODO Nullify 'self->recording'?
    self->mutex.unlock ();
}

VideoStream::Events const MediaRecorder::stream_events = {
    stream_audioMessage,
    stream_videoMessage,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
    stream_closed,
    NULL /* numWatchersChanged */,
    NULL /* mediaDesc */
};

void
MediaRecorder::stream_audioMessage (AudioMessage * const mt_nonnull audio_msg,
                                    void         * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    MediaRecorder * const self = stream_ticket->media_recorder;

    self->mutex.lock ();
    if (stream_ticket != self->cur_stream_ticket) {
        self->mutex.unlock ();
        return;
    }

    self->recordAudioMessage (audio_msg);
    self->mutex.unlock ();
}

void
MediaRecorder::stream_videoMessage (VideoMessage * const mt_nonnull video_msg,
                                    void         * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    MediaRecorder * const self = stream_ticket->media_recorder;

    self->mutex.lock ();
    if (stream_ticket != self->cur_stream_ticket) {
        self->mutex.unlock ();
        return;
    }

    self->recordVideoMessage (video_msg);
    self->mutex.unlock ();
}

void
MediaRecorder::stream_closed (void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    MediaRecorder * const self = stream_ticket->media_recorder;

    self->mutex.lock ();
    if (stream_ticket != self->cur_stream_ticket) {
        self->mutex.unlock ();
        return;
    }

//    logD_ (_func_);

    if (self->cur_stream)
        self->cur_stream->getEventInformer()->unsubscribe (self->stream_sbn);

    self->cur_stream_ticket = NULL;
    self->cur_stream = NULL;
    self->stream_sbn = NULL;

    self->recording = NULL;

    self->mutex.unlock ();
}

void
MediaRecorder::setVideoStream (VideoStream * const stream)
{
//    logD_ (_this_func, "stream 0x", fmt_hex, (UintPtr) stream);

    Ref<StreamTicket> const new_ticket = grabNew <StreamTicket> ();
    new_ticket->media_recorder = this;

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
                CbDesc<VideoStream::Events> (&stream_events, new_ticket, this, new_ticket));
    }

    cur_stream_ticket = new_ticket;
    cur_stream = stream;

    recordStreamHeaders ();

    mutex.unlock ();
}

void
MediaRecorder::startRecording ()
{
    mutex.lock ();
    doStartRecording (getUnixtime() * 1000000000);
    mutex.unlock ();
}

void
MediaRecorder::stopRecording ()
{
    mutex.lock ();

    if (postwrite_nanosec == 0 || postwrite_num_frames_limit == 0) {
        recording = NULL;
    } else
    if (!postwrite_active) {
        logD_ (_func, "postwrite begin");

        postwrite_active = true;
        postwrite_frame_counter = 0;

        if (got_unixtime_offset) {
            got_postwrite_start_ts = true;
            postwrite_start_ts_nanosec = prv_unixtime_dts_nanosec;
        } else {
            got_postwrite_start_ts = false;
            postwrite_start_ts_nanosec = 0;
        }
    }

    mutex.unlock ();
}

bool
MediaRecorder::isRecording ()
{
    mutex.lock ();
    bool const res = (bool) recording && !postwrite_active;
    mutex.unlock ();
    return res;
}

void
MediaRecorder::init (PagePool            * const page_pool,
                     ServerThreadContext * const mt_nonnull thread_ctx,
                     Vfs                 * const mt_nonnull vfs,
                     NamingScheme        * const mt_nonnull naming_scheme,
                     ConstMemory           const channel_name,
                     Time                  const prewrite_nanosec,
                     Count                 const prewrite_num_frames_limit,
                     Time                  const postwrite_nanosec,
                     Count                 const postwrite_num_frames_limit)
{
    this->page_pool = page_pool;
    this->thread_ctx = thread_ctx;
    this->vfs = vfs;
    this->naming_scheme = naming_scheme;
    this->channel_name = newString (channel_name);
    this->prewrite_nanosec  = prewrite_nanosec;
    this->prewrite_num_frames_limit = prewrite_num_frames_limit;
    this->postwrite_nanosec = postwrite_nanosec;
    this->postwrite_num_frames_limit = postwrite_num_frames_limit;
}

MediaRecorder::MediaRecorder (EmbedContainer * const embed_container)
    : Object                         (embed_container),
      prewrite_nanosec               (0),
      prewrite_num_frames_limit      (0),
      postwrite_nanosec              (0),
      postwrite_num_frames_limit     (0),
      got_unixtime_offset            (false),
      unixtime_offset_nanosec        (0),
      prv_unixtime_dts_nanosec       (0),
      got_pending_aac_seq_hdr        (false),
      got_pending_avc_seq_hdr        (false),
      got_avc_seq_hdr                (false),
      next_idx_unixtime_nanosec      (0),
      next_file_unixtime_nanosec     (0),
      postwrite_active               (false),
      got_postwrite_start_ts         (false),
      postwrite_start_ts_nanosec     (0),
      postwrite_frame_counter        (0),
      prewrite_queue_size            (0)
{
}

MediaRecorder::~MediaRecorder ()
{
    if (got_pending_aac_seq_hdr) {
        pending_aac_seq_hdr.release ();
        got_pending_aac_seq_hdr = false;
    }

    if (got_pending_avc_seq_hdr) {
        pending_avc_seq_hdr.release ();
        got_pending_avc_seq_hdr = false;
    }
}

}

