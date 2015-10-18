/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/get_file_session.h>


namespace MomentNvr {

static LogGroup libMary_logGroup_getfile ("mod_nvr.get_file", LogLevel::I);

MediaReader::ReadFrameBackend const GetFileSession::read_frame_backend = {
    audioFrame,
    videoFrame
};

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::audioFrame (AudioMessage * const mt_nonnull msg,
                            void         * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

//    logD (getfile, _func, " dts ", msg->dts_nanosec, " ", msg->frame_type);

    if (self->session_state == SessionState_Header) {
        if (msg->dts_nanosec > (self->start_unixtime_sec + self->duration_sec) * 1000000000) {
            logD (getfile, _func, "Finish");
            return MediaReader::ReadFrameResult_Finish;
        }

        if (msg->frame_type == AudioFrameType::AacSequenceHeader) {
            self->mp4_muxer.pass1_aacSequenceHeader (msg->page_pool,
                                                     msg->page_list.first,
                                                     msg->msg_offset,
                                                     msg->msg_len);
        } else
        if (msg->frame_type.isAudioData()) {
            self->last_audio_ts_nanosec = msg->dts_nanosec;
            self->got_last_audio_ts = true;

            self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Audio,
                                         msg->pts_nanosec,
                                         msg->dts_nanosec,
                                         msg->msg_len,
                                         false /* is_sync_sample */);

            ++self->total_num_frames;
        }
    } else {
        if (msg->frame_type.isAudioData()) {
            if (!self->got_last_audio_ts) {
                logD (getfile, _func, "Finish");
                return MediaReader::ReadFrameResult_Finish;
            }

            return self->doSendFrame (msg, self->last_audio_ts_nanosec);
        }
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::videoFrame (VideoMessage * const mt_nonnull msg,
                            void         * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

//    logD_ (_func, (self->session_state == SessionState_Header ? "HEADER" : "DATA"),
//           " dts ", msg->dts_nanosec, " ", msg->frame_type, " len ", msg->msg_len);

    assert (self->session_state != SessionState_Complete);

    if (self->session_state == SessionState_Header) {
        if (msg->dts_nanosec > (self->start_unixtime_sec + self->duration_sec) * 1000000000) {
            logD (getfile, _func, "Finish");
            return MediaReader::ReadFrameResult_Finish;
        }

        if (msg->frame_type == VideoFrameType::AvcSequenceHeader) {
            self->mp4_muxer.pass1_avcSequenceHeader (msg->page_pool,
                                                     msg->page_list.first,
                                                     msg->msg_offset,
                                                     msg->msg_len);
        } else
        if (msg->frame_type.isVideoData()) {
            self->last_video_ts_nanosec = msg->dts_nanosec;
            self->got_last_video_ts = true;

            self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Video,
                                         msg->pts_nanosec,
                                         msg->dts_nanosec,
                                         msg->msg_len,
                                         msg->frame_type.isKeyFrame());

            ++self->total_num_frames;
        }
    } else {
        if (msg->frame_type.isVideoData()) {
            if (!self->got_last_video_ts) {
                logD (getfile, _func, "Finish");
                return MediaReader::ReadFrameResult_Finish;
            }

            return self->doSendFrame (msg, self->last_video_ts_nanosec);
        }
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::doSendFrame (MediaMessage * const mt_nonnull msg,
                             Time           const /* last_ts_nanosec */)
{
    SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
    msg_pages->init (msg->page_list.first, msg->page_pool, msg->msg_offset, msg->msg_len);
    msg->page_pool->msgRef (msg->page_list.first);

    bool burst_limit = false;
    bool sender_limit = false;

    sender->lock ();
    sender->sendMessage_locked (msg_pages, true /* do_flush */);
    {
        SenderState const send_state = sender->getSenderState_locked();
        if (send_state != SenderState::ConnectionReady &&
            send_state != SenderState::ConnectionOverloaded)
        {
            logD (getfile, _func, "Connection overloaded: ", (unsigned) send_state);

            if (bps_limit_timer) {
                thread_ctx->getTimers()->deleteTimer (bps_limit_timer);
                bps_limit_timer = NULL;
            }

            burst_limit = true;
            sender_limit = true;
        }
    }
    sender->unlock ();

    bytes_transferred += msg->msg_len;

//    if (msg->dts_nanosec >= last_ts_nanosec) {
    ++pass2_num_frames;
    if (pass2_num_frames >= total_num_frames) {
        logD (getfile, _func, "Finish");
        return MediaReader::ReadFrameResult_Finish;
    }

    if (!burst_limit) {
        if (first_data_read) {
            if (first_burst_size
                && bytes_transferred >= first_burst_size)
            {
                logD (getfile, _func, "first_burst_size limit");
                burst_limit = true;
            }
        } else
        if (bps_limit) {
            Time const cur_time_millisec = getTimeMilliseconds();
            if (cur_time_millisec <= transfer_start_time_millisec
                || (double) bytes_transferred /
                           ((double) (cur_time_millisec - transfer_start_time_millisec) / 1000.0)
                       >= (double) bps_limit)
            {
                logD (getfile, _func, "bps_limit");
                burst_limit = true;
            }
        }
    }

    if (burst_limit) {
        if (!sender_limit) {
            assert (!bps_limit_timer);
            bps_limit_timer =
                    thread_ctx->getTimers()->addTimer (
                            CbDesc<Timers::TimerCallback> (bpsLimitTimerTick, this, this),
                            1     /* time_seconds */,
                            false /* periodical */,
                            true  /* auto_delete */);
        }
        return MediaReader::ReadFrameResult_BurstLimit;
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) bool
GetFileSession::senderClosedTask (void * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    logD_ (_func_);
    self->session_state = SessionState_Complete;
    return false /* do not reschedule */;
}

mt_sync_domain (readTask) bool
GetFileSession::readTask (void * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    logD (getfile, _func_);

    if (self->session_state == SessionState_Header) {
        MOMENT_SERVER__HEADERS_DATE

        for (;;) {
            MediaReader::ReadFrameResult const res = self->media_reader->readMoreData (&read_frame_backend, self);
            if (res == MediaReader::ReadFrameResult_Failure) {
                logE_ (_func, "ReadFrameResult_Failure");

                ConstMemory msg = "Data retrieval error";
                self->sender->send (self->page_pool,
                                    true /* do_flush */,
                                    // TODO No cache
                                    MOMENT_SERVER__500_HEADERS (msg.len()),
                                    "\r\n",
                                    msg);

                if (!self->req_is_keepalive)
                    self->sender->closeAfterFlush ();

                logA_ ("mod_nvr 500 ", self->req_client_addr, " ", self->req_request_line);
                return false /* do not reschedule */;
            }

            bool header_done = false;
            if (res == MediaReader::ReadFrameResult_NoData) {
                logD (getfile, _func, "ReadFrameResult_NoData");

                if (!self->got_last_audio_ts &&
                    !self->got_last_video_ts)
                {
                    ConstMemory msg = "Requested video data not found";
                    self->sender->send (self->page_pool,
                                        true /* do_flush */,
                                        // TODO No cache
                                        MOMENT_SERVER__404_HEADERS (msg.len()),
                                        "\r\n",
                                        msg);

                    if (!self->req_is_keepalive)
                        self->sender->closeAfterFlush ();

                    logA_ ("mod_nvr 404 ", self->req_client_addr, " ", self->req_request_line);
                    return false /* do not reschedule */;
                }

                header_done = true;
            } else
            if (res == MediaReader::ReadFrameResult_Finish) {
                logD (getfile, _func, "ReadFrameResult_Finish");
                header_done = true;
            }

            if (header_done) {
                self->session_state = SessionState_Data;
                self->media_reader->reset ();
                break;
            }

            assert (res != MediaReader::ReadFrameResult_BurstLimit);
            assert (res == MediaReader::ReadFrameResult_Success);
        }

        PagePool::PageListInfo const mp4_header = self->mp4_muxer.pass1_complete (self->duration_sec * 1000);

        {
            self->sender->send (self->page_pool,
                                true /* do_flush */,
                                // TODO No cache
                                MOMENT_SERVER__OK_HEADERS (
                                        (!self->octet_stream_mime ? ConstMemory ("video/mp4") :
                                                                    ConstMemory ("application/octet-stream")),
                                        mp4_header.data_len + self->mp4_muxer.getTotalDataSize()),
                                "\r\n");
            logD_ (_func, "CONTENT-LENGTH: ", mp4_header.data_len + self->mp4_muxer.getTotalDataSize());

            if (!self->req_is_keepalive)
                self->sender->closeAfterFlush ();

            logA_ ("mod_nvr 200 ", self->req_client_addr, " ", self->req_request_line);
        }

        {
            SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
            msg_pages->init (mp4_header.first, self->page_pool, /*msg_offs=*/ 0, mp4_header.data_len);

            self->sender->sendMessage (msg_pages, true /* do_flush */);
        }

        self->transfer_start_time_millisec = getTimeMilliseconds();
        self->bytes_transferred += mp4_header.data_len;

        self->sender->getEventInformer()->subscribe (
                CbDesc<Sender::Frontend> (&sender_frontend, self, self));
    }

    if (self->session_state == SessionState_Data) {
        self->doReadData ();
        self->first_data_read = false;
    }

    return false /* do not reschedule */;
}

mt_sync_domain (readTask) void
GetFileSession::bpsLimitTimerTick (void * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    self->thread_ctx->getTimers()->deleteTimer (self->bps_limit_timer);
    self->bps_limit_timer = NULL;

    self->doReadData ();
}

mt_sync_domain (readTask) void
GetFileSession::doReadData ()
{
    if (session_state == SessionState_Complete) {
        logD (getfile, _func, "SessionState_Complete");
        return;
    }

    for (;;) {
        MediaReader::ReadFrameResult const res = media_reader->readMoreData (&read_frame_backend, this);
        if (res == MediaReader::ReadFrameResult_Failure) {
            logE_ (_func, "ReadFrameResult_Failure");
            session_state = SessionState_Complete;
            if (!req_is_keepalive) {
                sender->closeAfterFlush ();
            }
            media_reader->reset ();
            return;
        }

        if (res == MediaReader::ReadFrameResult_BurstLimit) {
            logD (getfile, _func, "ReadFrameResult_BurstLimit");
            return;
        }

        bool data_done = false;
        if (res == MediaReader::ReadFrameResult_NoData) {
            logD (getfile, _func, "ReadFrameResult_NoData");
            data_done = true;
        } else
        if (res == MediaReader::ReadFrameResult_Finish) {
            logD (getfile, _func, "ReadFrameResult_Finish");
            data_done = true;
        }

        if (data_done) {
            session_state = SessionState_Complete;
            if (!req_is_keepalive) {
                sender->closeAfterFlush ();
            }
            media_reader->reset ();
            break;
        }

        assert (res == MediaReader::ReadFrameResult_Success);
    }
}

Sender::Frontend const GetFileSession::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
GetFileSession::senderStateChanged (SenderState   const send_state,
                                    void        * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    if (self->bps_limit_timer)
        return;

    if (send_state == SenderState::ConnectionReady)
        self->deferred_reg.scheduleTask (&self->read_task, false /* permanent */);
}

void
GetFileSession::senderClosed (Exception * const /* exc_ */,
                              void      * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    self->deferred_reg.scheduleTask (&self->sender_closed_task, false /* permanent */);
}

void
GetFileSession::start ()
{
    mutex.lock ();
    started = true;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
}

mt_const void
GetFileSession::init (MomentServer * const mt_nonnull moment,
                      HttpRequest  * const mt_nonnull req,
                      Sender       * const mt_nonnull sender,
                      PagePool     * const page_pool,
                      Vfs          * const vfs,
                      ConstMemory    const stream_name,
                      Time           const start_unixtime_sec,
                      Time           const duration_sec,
                      bool           const octet_stream_mime,
                      CbDesc<Frontend> const &frontend)
{
    this->moment = moment;
    this->page_pool = page_pool;
    this->sender = sender;
    this->frontend = frontend;

    this->start_unixtime_sec = start_unixtime_sec;
    this->duration_sec = duration_sec;

    this->octet_stream_mime = octet_stream_mime;

    this->req_is_keepalive = req->getKeepalive();
    this->req_client_addr = req->getClientAddress();
    this->req_request_line = st_grab (new (std::nothrow) String (req->getRequestLine()));

    media_reader->init (page_pool,
                        vfs,
                        stream_name,
                        start_unixtime_sec,
                        0 /* burst_size_limit */);

    mp4_muxer.init (page_pool, true /* derive_dts_from_pts */); // TODO Don't derive DTS from PTS

    thread_ctx = moment->getReaderThreadPool()->grabThreadContext (stream_name);
    if (thread_ctx) {
        reader_thread_ctx = thread_ctx;
    } else {
        logE_ (_func, "Could not get reader thread context: ", exc->toString());
        reader_thread_ctx = NULL;
        thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
    }

    deferred_reg.setDeferredProcessor (thread_ctx->getDeferredProcessor());
}

GetFileSession::GetFileSession (EmbedContainer * const embed_container)
    : Object (embed_container),
      start_unixtime_sec (0),
      duration_sec       (0),
      octet_stream_mime  (false),
      req_is_keepalive   (false),
      first_burst_size   (1 << 20 /* 1 MB */),
      bps_limit          (0),
      transfer_start_time_millisec (0),
      bytes_transferred  (0),
      reader_thread_ctx  (NULL),
      media_reader       (this /* embed_container */),
      session_state      (SessionState_Header),
      first_data_read    (false),
      got_last_audio_ts  (false),
      last_audio_ts_nanosec (0),
      got_last_video_ts  (false),
      last_video_ts_nanosec (0),
      total_num_frames   (0),
      pass2_num_frames   (0),
      started            (false)
{
    read_task.cb  = CbDesc<DeferredProcessor::TaskCallback> (readTask, this, this);
    sender_closed_task.cb = CbDesc<DeferredProcessor::TaskCallback> (senderClosedTask, this, this);
}

GetFileSession::~GetFileSession ()
{
    deferred_reg.release ();

    if (reader_thread_ctx) {
        moment->getReaderThreadPool()->releaseThreadContext (reader_thread_ctx);
        reader_thread_ctx = NULL;
    }
}

}

