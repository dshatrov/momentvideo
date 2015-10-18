/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/libmoment.h>
#include <moment/inc.h>

#include <moment-rtsp/rtp_port_pair.h>
#include <moment-rtsp/util_rtsp.h>

#include <moment-rtsp/rtsp_service.h>


//#warning TCP interleaved conn closed -> treat like TEARDOWN (sometimes doesn't happen).

//#warning In TCP only mode, don't reply errors for Android clients


// Common maximum non-fragmented UDP packet payload size:
//     MTU 1500 - 20 bytes for IPv4 - 8 bytes for UDP
//     ^ TODO What about IPv6? Make expected max UDP payload configurable.
//
#define MOMENT_RTSP__MAX_UDP_PAYLOAD 1472

#define MOMENT_RTSP__RATELIMIT_THRESHOLD_MICROSEC 3000000


namespace MomentRtsp {

static LogGroup libMary_logGroup_rtsp_service ("rtsp_service", LogLevel::I);

RtspService::RtpServerSockets::RtpServerSockets (EmbedContainer * const embed_container)
    : Object      (embed_container),
      rtp_socket  (this /* embed_container */),
      rtcp_socket (this /* embed_container */)
{}

RtspService::TcpConnectionBlock::TcpConnectionBlock (EmbedContainer * const embed_container)
    : Object      (embed_container),
      tcp_conn    (this /* embed_container */),
      receiver    (this /* embed_container */),
      sender      (this /* embed_container */),
      http_server (this /* embed_container */)
{}

RtspService::RtpDataSession::RtpDataSession (EmbedContainer * const embed_container)
    : Object           (embed_container),
      ssrc             (0),
      tcp_interleaved  (false),
      tcp_rtp_channel  (0),
      tcp_rtcp_channel (0),
      rtp_port         (0),
      rtcp_port        (0),
      rtp_sender       (this /* embed_container */, true /* datagram_mode */),
      rtcp_sender      (this /* embed_container */, true /* datagram_mode */),
      first_sender_report_time_microsec (0),
      first_sender_report_sent (false),
      packet_count     (0),
      octet_count      (0)
{}

RtspService::RtspSession::RtspSession (EmbedContainer * const embed_container)
    : Object                     (embed_container),
      valid                      (true),
      last_request_time_millisec (0),
      playing                    (false),
      session_list_el            (NULL),
      got_timestamp_offset       (false),
      timestamp_offset           (0),
      last_timestamp             (0)
{}

RtspService::RtspConnection::RtspConnection (EmbedContainer * const embed_container)
    : Object                     (embed_container),
      valid                      (true),
      last_request_time_millisec (0),
      receiver_mapping           (NULL),
      start_time_millisec        (0),
      got_start_time             (false),
      duration_millisec          (0),
      got_duration               (false),
      audio_rate                 (44100),
      data_state                 (DataState_Header),
      msg_len                    (0),
      got_len                    (0),
      conn_list_el               (NULL)
{}

// TODO Make minusOneWatcher() a deferred notification in VideoStream
//      and get rid of this.
static void
deferredMinusOneWatcher (void *_stream)
{
    VideoStream * const stream = static_cast <VideoStream*> (_stream);
    stream->minusOneWatcher ();
}

mt_mutex (mutex) void
RtspService::destroyRtspSession (RtspSession * const mt_nonnull session)
{
    logD (rtsp_service, _this_func, "session 0x", fmt_hex, (UintPtr) session);

    if (!session->valid) {
        return;
    }
    session->valid = false;

    switch ((MediaSource::Type::Value) session->media_source->type) {
        case MediaSource::Type::MediaStream: {
            MediaStream * const stream = static_cast <MediaStream*> (session->media_source.ptr());
            destroyRtspSession_handleMediaStream (session, stream);
        } break;
        case MediaSource::Type::VodSource: {
            VodSource * const vod_source = static_cast <VodSource*> (session->media_source.ptr());
            destroyRtspSession_handleVodSource (session, vod_source);
        } break;
    }

    rtsp_session_cleanup_list.remove (session);
    session_list.remove (session->session_list_el);
    /* session is no more */
}

mt_mutex (mutex) void
RtspService::destroyRtspSession_handleMediaStream (RtspSession * const mt_nonnull session,
                                                   MediaStream * const mt_nonnull stream)
{
    if (session->stream_sbn) {
        stream->getEventInformer()->unsubscribe (session->stream_sbn);
        session->stream_sbn = NULL;
    }

    if (session->playing && stream) {
        Cb< void (void*) > cb (deferredMinusOneWatcher, stream, NULL);
        cb.call_deferred (&deferred_reg, deferredMinusOneWatcher, stream /* extra_ref_data */);
    }
}

mt_mutex (mutex) void
RtspService::destroyRtspSession_handleVodSource (RtspSession * const mt_nonnull /* session */,
                                                 VodSource   * const mt_nonnull /* vod_source */)
{
  // TODO
}

mt_mutex (mutex) void
RtspService::destroyRtspConnection (RtspConnection * const mt_nonnull rtsp_conn)
{
    logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn);

    if (!rtsp_conn->valid) {
        return;
    }
    rtsp_conn->valid = false;

    if (rtsp_conn->conn_keepalive_timer) {
        thread_ctx->getTimers()->deleteTimer (rtsp_conn->conn_keepalive_timer);
        rtsp_conn->conn_keepalive_timer = NULL;
    }

    if (rtsp_conn->pollable_key) {
        thread_ctx->getPollGroup()->removePollable (rtsp_conn->pollable_key);
        rtsp_conn->pollable_key = NULL;
    }

//#warning delete RtspSessions if tcp_interleaved

    rtsp_conn_cleanup_list.remove (rtsp_conn);
    conn_list.remove (rtsp_conn->conn_list_el);
    /* rtsp_conn is no more */
}

void
RtspService::senderReportTick (void * const _session)
{
    RtspSession * const session = static_cast <RtspSession*> (_session);
    Ref<RtspService> const self = session->weak_rtsp_service.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    if (!session->valid) {
        self->mutex.unlock ();
        return;
    }

#if 0
//#warning FIXME mutex sync
    if (!session->got_timestamp_offset) {
        self->mutex.unlock ();
        return;
    }
#endif

    self->mutex.unlock ();

    if (session->audio_data_session) {
        logD (rtsp_service, _func, "sending audio RTCP SR");
        self->sendRtcpSenderReport (session->audio_data_session);
    }

    if (session->video_data_session) {
        logD (rtsp_service, _func, "sending video RTCP SR");
        self->sendRtcpSenderReport (session->video_data_session);
    }
}

void
RtspService::timeoutCheckTick (void * const _self)
{
    RtspService * const self = static_cast <RtspService*> (_self);

    Time const cur_time_millisec = getTimeMilliseconds();

    self->mutex.lock ();

    if (self->session_request_timeout_millisec) {
        RtspSessionCleanupList::iterator iter (self->rtsp_session_cleanup_list);
        while (!iter.done()) {
            RtspSession * const session = iter.next ();
            if (session->last_request_time_millisec <= cur_time_millisec) {
                if (cur_time_millisec - session->last_request_time_millisec < self->session_request_timeout_millisec)
                    break;

                logD (rtsp_service, _func, "destroying session by timeout");
                self->destroyRtspSession (session);
            } else {
                session->last_request_time_millisec = cur_time_millisec;
            }
        }
    }

    if (self->conn_request_timeout_millisec) {
        RtspConnectionCleanupList::iterator iter (self->rtsp_conn_cleanup_list);
        while (!iter.done()) {
            RtspConnection * const rtsp_conn = iter.next ();
            if (rtsp_conn->last_request_time_millisec <= cur_time_millisec) {
                if (cur_time_millisec - rtsp_conn->last_request_time_millisec < self->conn_request_timeout_millisec)
                    break;

                logD (rtsp_service, _func, "destroying connection by timeout");
                self->destroyRtspConnection (rtsp_conn);
            } else {
                rtsp_conn->last_request_time_millisec = cur_time_millisec;
            }
        }
    }

    self->mutex.unlock ();
}

VideoStream::Events const RtspService::stream_events = {
    stream_audioMessage,
    stream_videoMessage,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
    stream_closed,
    NULL /* numWatchersChanged */,
    NULL /* mediaDesc */
};

void
RtspService::stream_audioMessage (AudioMessage * const mt_nonnull audio_msg,
                                  void         * const _bound_sessions)
{
    logS (rtsp_service, _funcp (_bound_sessions));

    if (!audio_msg->frame_type.isAudioData()) {
        logD (rtsp_service, _funcp (_bound_sessions) "not audio data");
        return;
    }

    BoundSessions * const bound_sessions = static_cast <BoundSessions*> (_bound_sessions);
    Ref<RtspSession> const session = bound_sessions->weak_audio_rtsp_session.getRef ();
    if (!session) {
        logD (rtsp_service, _funcp (_bound_sessions) "session gone");
        return;
    }

    stream_doAudioMessage (audio_msg, session);
}

void
RtspService::stream_doAudioMessage (AudioMessage * const mt_nonnull audio_msg,
                                    RtspSession  * const mt_nonnull session)
{
    // Assuming that 'session->audio_data_session' is mt_const at this point.
    if (!session->audio_data_session) {
        logD (rtsp_service, _funcp (session) "null audio_data_session");
        return;
    }
    RtpDataSession * const data_session = session->audio_data_session;

    if (mt_unlikely (!data_session->tcp_interleaved && data_session->rtp_port == 0))
        return;

    Size const header_len = 12 + 4 + (data_session->tcp_interleaved ? 4 : 0);

    SenderMessageEntry_Pages * const msg_pages =
	    SenderMessageEntry_Pages::createNew (header_len /* max_header_len */);
    msg_pages->init (audio_msg->page_list.first, audio_msg->page_pool, audio_msg->msg_offset, audio_msg->msg_len);
    audio_msg->page_pool->msgRef (audio_msg->page_list.first);

    msg_pages->header_len = header_len;
    {
        Byte * const hdr = msg_pages->getHeaderData();

        Size offs = 0;
        if (data_session->tcp_interleaved) {
            hdr [0] = 0x24 /* '$' */;
            hdr [1] = (Byte) data_session->tcp_rtp_channel;

            Size const len = audio_msg->msg_len + 12 + 4;
            hdr [2] = (Byte) ((len >> 8) & 0xff);
            hdr [3] = (Byte) ((len >> 0) & 0xff);

            offs = 4;
        }

        hdr [offs + 0] = (2 << 6);
        hdr [offs + 1] = 97;
        // marker bit
        hdr [offs + 1] |= 0x80;

        hdr [offs + 2] = (Byte) ((data_session->sequence_number >> 8) & 0xff);
        hdr [offs + 3] = (Byte) ((data_session->sequence_number >> 0) & 0xff);
        ++data_session->sequence_number;

        if (!session->got_timestamp_offset) {
            session->got_timestamp_offset = true;
            session->timestamp_offset = audio_msg->dts_nanosec;
        }
        session->last_timestamp = audio_msg->pts_nanosec - session->timestamp_offset;
//#warning TODO Handle PTS properly.
        Uint32 const ts = (Uint32) ((audio_msg->pts_nanosec - session->timestamp_offset) / 1000 * data_session->freq / 10000 + 1000000);

        hdr [offs + 4] = (Byte) ((ts >> 24) & 0xff);
        hdr [offs + 5] = (Byte) ((ts >> 16) & 0xff);
        hdr [offs + 6] = (Byte) ((ts >>  8) & 0xff);
        hdr [offs + 7] = (Byte) ((ts >>  0) & 0xff);

        hdr [offs +  8] = (Byte) ((data_session->ssrc >> 24) & 0xff);
        hdr [offs +  9] = (Byte) ((data_session->ssrc >> 16) & 0xff);
        hdr [offs + 10] = (Byte) ((data_session->ssrc >>  8) & 0xff);
        hdr [offs + 11] = (Byte) ((data_session->ssrc >>  0) & 0xff);

        hdr [offs + 12] = 0x00;
        hdr [offs + 13] = 0x10;
        hdr [offs + 14] = (audio_msg->msg_len >> 5) & 0xff;
        hdr [offs + 15] = (audio_msg->msg_len << 3) & 0xff;
    }

    {
//#warning TODO If a mutex is necessary, it should be a DataSession mutex
        Ref<RtspService> const self = session->weak_rtsp_service.getRef ();
        if (self) {
            self->mutex.lock ();
            ++data_session->packet_count;
            data_session->octet_count += audio_msg->msg_len + 4;
            self->mutex.unlock ();
        }
    }

    if (!data_session->tcp_interleaved)
        data_session->rtp_sender->sendMessage (msg_pages, true /* do_flush */);
    else
        data_session->tcp_block->sender->sendMessage (msg_pages, true /* do_flush */);
}

namespace {
    // Note: This sender is only suitable for UDP.
    //       It doesn't support partial writes.
    class RtpSingleNal_SenderMessage : public SenderMessageEntry_Common
    {
    public:
        Byte* getHeaderData () { return (Byte*) this + sizeof (*this); }

      mt_iface (SenderMessageEntry)
        Size getTotalMsgLen () const;

        bool isEmpty ();

        void fillIovs (IovArray *iovs,
                       Size      max_iovs);

        bool react (Size  bytes_sent,
                    Size * mt_nonnull ret_accepted);

        void release ();
      mt_iface_end

        static RtpSingleNal_SenderMessage* createNew (Size const header_len)
        {
            Byte * const buf = new (std::nothrow) Byte [sizeof (RtpSingleNal_SenderMessage) + header_len];
            assert (buf);
            return new (buf) RtpSingleNal_SenderMessage;
        }
    };
}

Size
RtpSingleNal_SenderMessage::getTotalMsgLen () const
{
    return header_len + msg_len;
}

bool
RtpSingleNal_SenderMessage::isEmpty ()
{
    return (header_len == 0) && (msg_len == 0);
}

void
RtpSingleNal_SenderMessage::fillIovs (IovArray * const iovs,
                                      Size       const max_iovs)
{
    Size i = 0;

    if (header_len > 0) {
        iovs->addIov (getHeaderData(), header_len);

        ++i;
        assert (i <= max_iovs);
    }

    PagePool::Page *page = first_page;
    Size left = msg_len;
    while (page) {
        Size const offs = (page == first_page ? msg_offs : 0);
        Size const len = (left <= page->data_len - offs) ? left : page->data_len - offs;

        if (len > 0) {
            iovs->addIov (page->getData() + offs, len);

            ++i;
            assert (i <= max_iovs);
        }

        left -= len;
        page = page->getNextMsgPage();
    }
}

bool
RtpSingleNal_SenderMessage::react (Size   const bytes_sent,
                                   Size * const mt_nonnull ret_accepted)
{
    Size const total_len = getTotalMsgLen();
    *ret_accepted = (bytes_sent <= total_len ? bytes_sent : total_len);

    if (bytes_sent == 0 && total_len > 0)
        return false;

    return true;
}

void
RtpSingleNal_SenderMessage::release ()
{
    PagePool::Page *page = first_page;
    Size left = msg_len;
    while (left > 0) {
        PagePool::Page * const next_page = page->getNextMsgPage();

        Size const offs = (page == first_page ? msg_offs : 0);
        Size const len = (left <= page->data_len - offs) ? left : page->data_len - offs;

        page_pool->pageUnref (page);

        left -= len;
        page = next_page;
    }

    this->~RtpSingleNal_SenderMessage ();
    delete[] (Byte*) this;
}

void
RtspService::sendRtcpSenderReport (RtpDataSession * const mt_nonnull data_session)
{
//#error Проблемы: 1. Синхронизация в режиме TCP interleaved. Видимо, дело в формировании Sender reports.

//#error           2. Поддерживает ли Android режим TCP interleaved в дефолтном плеере? - возможно, требует доп. трюков - сравни с live555 (блокируй UDP)
//                    ^^^ Прочитал исходники StageFright. НЕТ способа заставить этот клиент использовать TCP interleaved без таймаута.

//#error TEST RTCP in TCP interleaved mode somehow screws synchronization
//    if (data_session->tcp_interleaved)
//        return;

    if (!data_session->tcp_interleaved && data_session->rtcp_port == 0)
        return;

    updateTime ();
    Time const time_microsec = getTimeMicroseconds ();

    Byte msg [52 + 4];

    Size offs = 0;
    if (data_session->tcp_interleaved) {
        msg [0] = 0x24 /* '$' */;
        msg [1] = (Byte) data_session->tcp_rtcp_channel;

        msg [2] =  0;
        msg [3] = 52;

        offs = 4;
    }

    msg [offs + 0] = 0x80;
    msg [offs + 1] = 200 /* SR */;

    // length
    msg [offs + 2] = 0;
    msg [offs + 3] = 6;

    // SSRC
    msg [offs + 4] = (Byte) ((data_session->ssrc >> 24) & 0xff);
    msg [offs + 5] = (Byte) ((data_session->ssrc >> 16) & 0xff);
    msg [offs + 6] = (Byte) ((data_session->ssrc >>  8) & 0xff);
    msg [offs + 7] = (Byte) ((data_session->ssrc >>  0) & 0xff);

    {
        // NTP to UNIXTIME diff: 2208988800 (RFC 868)
        Uint64 const ntp_time_sec = time_microsec / 1000000 + 2208988800000000ull;
        // 2 ** 32 / 10 ** 6
        Uint64 const ntp_time_frac = (Uint64) ((double) time_microsec / 15625.0 * 0x04000000 + 0.5);

        msg [offs +  8] = (Byte) ((ntp_time_sec >> 24) & 0xff);
        msg [offs +  9] = (Byte) ((ntp_time_sec >> 16) & 0xff);
        msg [offs + 10] = (Byte) ((ntp_time_sec >>  8) & 0xff);
        msg [offs + 11] = (Byte) ((ntp_time_sec >>  0) & 0xff);

        msg [offs + 12] = (Byte) ((ntp_time_frac >> 24) & 0xff);
        msg [offs + 13] = (Byte) ((ntp_time_frac >> 16) & 0xff);
        msg [offs + 14] = (Byte) ((ntp_time_frac >>  8) & 0xff);
        msg [offs + 15] = (Byte) ((ntp_time_frac >>  0) & 0xff);
    }

    mutex.lock ();
    if (!data_session->first_sender_report_sent) {
        data_session->first_sender_report_time_microsec = time_microsec;
        data_session->first_sender_report_sent = true;
    }
    Time const first_time_microsec = data_session->first_sender_report_time_microsec;
    Uint32 const packet_count = data_session->packet_count;
    Uint32 const octet_count  = data_session->octet_count;
    mutex.unlock ();

    // RTP timestamp
    {
        Uint64 const ts_microsec = time_microsec - first_time_microsec;
        Uint32 const ts = (Uint32) (ts_microsec * data_session->freq / 10000 + 1000000);

        msg [offs + 16] = (Byte) ((ts >> 24) & 0xff);
        msg [offs + 17] = (Byte) ((ts >> 16) & 0xff);
        msg [offs + 18] = (Byte) ((ts >>  8) & 0xff);
        msg [offs + 19] = (Byte) ((ts >>  0) & 0xff);
    }

    // sender's packet count
    msg [offs + 20] = (Byte) ((packet_count >> 24) & 0xff);
    msg [offs + 21] = (Byte) ((packet_count >> 16) & 0xff);
    msg [offs + 22] = (Byte) ((packet_count >>  8) & 0xff);
    msg [offs + 23] = (Byte) ((packet_count >>  0) & 0xff);

    // sender's octet count
    msg [offs + 24] = (Byte) ((octet_count >> 24) & 0xff);
    msg [offs + 25] = (Byte) ((octet_count >> 16) & 0xff);
    msg [offs + 26] = (Byte) ((octet_count >>  8) & 0xff);
    msg [offs + 27] = (Byte) ((octet_count >>  0) & 0xff);

    // SDES
    msg [offs + 28] = 0x81;
    msg [offs + 29] = 0xca;
    msg [offs + 30] = 0x00;
    msg [offs + 31] = 0x05;
    msg [offs + 32] = (Byte) ((data_session->ssrc >> 24) & 0xff);
    msg [offs + 33] = (Byte) ((data_session->ssrc >> 16) & 0xff);
    msg [offs + 34] = (Byte) ((data_session->ssrc >>  8) & 0xff);
    msg [offs + 35] = (Byte) ((data_session->ssrc >>  0) & 0xff);
    msg [offs + 36] = 0x01;
    msg [offs + 37] = 0x0a;
    msg [offs + 38] = 0x65;
    msg [offs + 39] = 0x72;
    msg [offs + 40] = 0x64;
    msg [offs + 41] = 0x69;
    msg [offs + 42] = 0x7a;
    msg [offs + 43] = 0x7a;
    msg [offs + 44] = 0x2d;
    msg [offs + 45] = 0x76;
    msg [offs + 46] = 0x64;
    msg [offs + 47] = 0x73;
    msg [offs + 48] = 0x00;
    msg [offs + 49] = 0x00;
    msg [offs + 50] = 0x00;
    msg [offs + 51] = 0x00;

    if (!data_session->tcp_interleaved) {
        data_session->rtcp_sender->send (page_pool, true /* do_flush */, ConstMemory (msg, 52 + offs));
    } else {
        data_session->tcp_block->sender->send (page_pool, true /* do_flush */, ConstMemory (msg, 52 + offs));
    }
}

//#warning Remove dependency on 'this' pointer, make this static, remove getRef() from stream_videoMessage()
mt_sync_domain (stream) void
RtspService::sendRtpVideoMessage (RtspSession    * const mt_nonnull session,
                                  ConstMemory      const extra_header,
                                  VideoMessage   * const mt_nonnull video_msg,
                                  PagePool       * const mt_nonnull normalized_page_pool,
                                  PagePool::Page * const page,
                                  Size             const data_offs,
                                  Size             const data_len,
                                  bool             const last_nal)
{
    logS (rtsp_service, _funcp (session) "video_msg: ", *video_msg);

    assert (session->video_data_session);
    RtpDataSession * const data_session = session->video_data_session;

    Byte *header;
    SenderMessageEntry_Common *msg = NULL;
    if (!data_session->tcp_interleaved) {
        RtpSingleNal_SenderMessage * const msg_ = RtpSingleNal_SenderMessage::createNew (18 /* header_len */);
        assert (msg_);
        msg = msg_;
        header = msg_->getHeaderData();
    } else {
        SenderMessageEntry_Pages * const msg_ = SenderMessageEntry_Pages::createNew (18 /* header_len */);
        assert (msg_);
        msg_->init (page, normalized_page_pool, data_offs, data_len);
        msg = msg_;
        header = msg_->getHeaderData();
    }

    msg->page_pool  = normalized_page_pool;
    msg->first_page = page;
    msg->msg_offs   = data_offs;
    msg->msg_len    = data_len;

    assert (4 + 12 + extra_header.len() <= 18);

    Size offs = 0;
    if (data_session->tcp_interleaved) {
        header [0] = 0x24 /* '$' */;
        header [1] = (Byte) data_session->tcp_rtp_channel;

        Size const len = data_len + 12 + extra_header.len();
        header [2] = (Byte) ((len >> 8) & 0xff);
        header [3] = (Byte) ((len >> 0) & 0xff);

        offs = 4;
    }

    header [offs + 0] = 0x80; // version (V)    : 2
                              // padding (P)    : 0
                              // extension (X)  : 0
                              // CRSC count (CC): 0
    header [offs + 1] = 96;   // payload type (PT)
    if (last_nal)
        header [offs + 1] |= 0x80; // marker (M)

    header [offs + 2] = (Byte) ((data_session->sequence_number >> 8) & 0xff);
    header [offs + 3] = (Byte) ((data_session->sequence_number >> 0) & 0xff);
    ++data_session->sequence_number;

    Uint64 ts_nanosec;
    if (!session->got_timestamp_offset) {
        if (video_msg->frame_type.hasTimestamp()) {
            session->got_timestamp_offset = true;
            session->timestamp_offset = video_msg->dts_nanosec;
        }

        ts_nanosec = video_msg->pts_nanosec - video_msg->dts_nanosec;
    } else {
        if (video_msg->frame_type.hasTimestamp())
            ts_nanosec = video_msg->pts_nanosec - session->timestamp_offset;
        else
            ts_nanosec = session->last_timestamp;
    }
    session->last_timestamp = ts_nanosec;
//#warning TODO Handle PTS properly.
    Uint32 const ts = (Uint32) (ts_nanosec / 10 * 9 / 10000 + 1000000);

    header [offs + 4] = (Byte) ((ts >> 24) & 0xff);
    header [offs + 5] = (Byte) ((ts >> 16) & 0xff);
    header [offs + 6] = (Byte) ((ts >>  8) & 0xff);
    header [offs + 7] = (Byte) ((ts >>  0) & 0xff);

    header [offs +  8] = (Byte) ((data_session->ssrc >> 24) & 0xff);
    header [offs +  9] = (Byte) ((data_session->ssrc >> 16) & 0xff);
    header [offs + 10] = (Byte) ((data_session->ssrc >>  8) & 0xff);
    header [offs + 11] = (Byte) ((data_session->ssrc >>  0) & 0xff);

    memcpy (header + offs + 12, extra_header.mem(), extra_header.len());

    msg->header_len = offs + 12 + extra_header.len();
    assert (msg->header_len <= 18);

    {
        PagePool::Page *page = msg->first_page;
        Size left = msg->msg_len;
        while (left > 0) {
            PagePool::Page * const next_page = page->getNextMsgPage();

            Size const page_offs = (page == msg->first_page ? msg->msg_offs : 0);
            Size const len = (left <= page->data_len - page_offs) ? left : page->data_len - page_offs;

            normalized_page_pool->pageRef (page);

            left -= len;
            page = next_page;
        }
    }

    mutex.lock ();
    ++data_session->packet_count;
    data_session->octet_count += data_len + extra_header.len();
    mutex.unlock ();

    if (!data_session->tcp_interleaved)
        data_session->rtp_sender->sendMessage (msg, false /* do_flush */);
    else
        data_session->tcp_block->sender->sendMessage (msg, false /* do_flush */);
}

void
RtspService::stream_videoMessage (VideoMessage * const mt_nonnull video_msg,
                                  void         * const _bound_sessions)
{
    logS (rtsp_service, _funcp (_bound_sessions));

    BoundSessions * const bound_sessions = static_cast <BoundSessions*> (_bound_sessions);
    Ref<RtspSession> const session = bound_sessions->weak_video_rtsp_session.getRef ();
    if (!session) {
        logD (rtsp_service, _funcp (_bound_sessions) "session gone");
        return;
    }

    stream_doVideoMessage (video_msg, session);
}

void
RtspService::stream_doVideoMessage (VideoMessage * const mt_nonnull video_msg,
                                    RtspSession  * const mt_nonnull session)
{
    // Assuming that 'session->video_data_session' is mt_const at this point.
    if (!session->video_data_session) {
        logD (rtsp_service, _funcp (session) "null video_data_session");
        return;
    }

    if (mt_unlikely (!session->video_data_session->tcp_interleaved && session->video_data_session->rtp_port == 0))
        return;

    if (video_msg->frame_type.isVideoData()) {
        if (!session->video_data_session->got_keyframe) {
            if (!video_msg->frame_type.isKeyFrame()) {
                logD (rtsp_service, _funcp (session) "waiting for keyframe");
                return;
            }
            session->video_data_session->got_keyframe = true;
        }
    }

    // TODO Is 'self' pointer really needed here? Obtaining it is not free.
    Ref<RtspService> const self = session->weak_rtsp_service.getRef ();
    if (!self) {
        logD (rtsp_service, _funcp (session) "self gone");
        return;
    }

    PagePool::PageListArray arr (video_msg->page_list.first, video_msg->msg_offset, video_msg->msg_len);
    Size pos = 0;
    for (;;) {
        if (pos + 4 > video_msg->msg_len) {
            if (mt_unlikely (pos != video_msg->msg_len)) {
                logD (rtsp_service, _func, "short nal length header: pos ", pos, ", msg_len ", video_msg->msg_len);
            }
            break;
        }

        Byte nal_len_buf [4];
        arr.get (pos, Memory::forObject (nal_len_buf));
        Uint32 const nal_len = readBe32 (nal_len_buf);
        pos += 4;
        if (mt_unlikely (nal_len == 0))
            continue;

        if (mt_unlikely (pos + nal_len > video_msg->msg_len)) {
            logD (rtsp_service, _func, "bad nal_len: ", nal_len, " (pos ", pos, ", msg_len ", video_msg->msg_len);
            break;
        }

        bool const last_nal = pos + nal_len + 4 > video_msg->msg_len;

        PagePool::Page * const page = arr.getNextPageToAccess ();
        Size const offs = arr.getNextInPageOffset ();

        // TODO Send single-time aggregation packets (STAP-A) when it's appropriate.

        // TODO Insert AU delimiters when needed.

        // TODO Insert codec data in front of IDR frames.

        // TODO NRI field should be set as suggested by RFC 6184.

        Size const max_rtp_payload = MOMENT_RTSP__MAX_UDP_PAYLOAD - 12 /* RTP header */;
        if (nal_len > max_rtp_payload) {
            Size const max_fua_payload = MOMENT_RTSP__MAX_UDP_PAYLOAD - 12 /* RTP */ - 2 /* FU-A */;
            //
            // RFC 6184: "A fragmented NAL unit MUST NOT be transmitted in one FU;
            // that is, the Start bit and End bit MUST NOT both be set to one in
            // the same FU header."
            //
            // ^ We comply by making sure that max_fua_payload is less than max_rtp_payload.
            //   Note that FU-A spares one byte (NAL unit header byte).
            //
            assert (max_fua_payload + 1 < max_rtp_payload);

            PagePool::PageListArray nal_arr (page, offs, nal_len);

            Size fragment_offs = 1; // NAL unit header byte is not included in FU-A payload.
            while (fragment_offs < nal_len) {
                Size const fragment_len =
                        (nal_len - fragment_offs >= max_fua_payload ? max_fua_payload : nal_len - fragment_offs);
                bool const last_fragment = (fragment_offs + fragment_len == nal_len);

                Byte header [2];
                header [0] =   (*(page->getData() + offs) & 0x60) /* NRI */
                             | 28 /* Type: FU-A */;
                header [1] =   (fragment_offs == 1 ? 0x80 : 0x00) /* Start bit */
                             | (last_fragment      ? 0x40 : 0x00) /* End bit */
                             | (*(page->getData() + offs) & 0x1f) /* NAL unit type */;

                {
                  // Updating nal_arr's "next" position.
                    Byte b;
                    nal_arr.get (fragment_offs - 1, Memory::forObject (b));
                }
                self->sendRtpVideoMessage (session,
                                           ConstMemory::forObject (header),
                                           video_msg,
                                           video_msg->page_pool,
                                           nal_arr.getNextPageToAccess(),
                                           nal_arr.getNextInPageOffset(),
                                           fragment_len,
                                           last_nal && last_fragment);

                fragment_offs += fragment_len;
            }
        } else {
            self->sendRtpVideoMessage (session,
                                       ConstMemory() /* extra_header */,
                                       video_msg,
                                       video_msg->page_pool,
                                       page,
                                       offs,
                                       nal_len,
                                       last_nal);
        }

        pos += nal_len;
    }

    if (!session->video_data_session->tcp_interleaved)
        session->video_data_session->rtp_sender->flush ();
    else
        session->video_data_session->tcp_block->sender->flush ();
}

void
RtspService::stream_doClosed (RtspSession * const mt_nonnull session)
{
    Ref<RtspService> const self = session->weak_rtsp_service.getRef ();
    if (!self)
        return;

  // TODO Send RTCP BYE packets for open data sessions.

    Ref<RtspConnection> const rtsp_conn = session->weak_rtsp_conn.getRef ();

    self->mutex.lock ();
    if (rtsp_conn) {
        self->destroyRtspConnection (rtsp_conn);
    }
    self->destroyRtspSession (session);
    self->mutex.unlock ();
}

void
RtspService::stream_closed (void * const _bound_sessions)
{
    BoundSessions * const bound_sessions = static_cast <BoundSessions*> (_bound_sessions);
    {
        Ref<RtspSession> const audio_session = bound_sessions->weak_audio_rtsp_session.getRef ();
        if (audio_session)
            stream_doClosed (audio_session);
    }
    {
        Ref<RtspSession> const video_session = bound_sessions->weak_video_rtsp_session.getRef ();
        if (video_session)
            stream_doClosed (video_session);
    }
}

VodSource::Frontend const RtspService::vod_source_frontend = {
    vod_mediaMessage,
    vod_endOfSource
};

struct RtspService::RateLimitTimerData : public Referenced
{
    enum Type {
        t_Audio,
        t_Video
    };

    Type const type;

    mt_const RtspService::RtspSession *session;
    mt_const PagePool *page_pool;

    RateLimitTimerData (Type const type)
        : type (type)
    {}
};

struct RtspService::RateLimitTimerData_Audio : public RateLimitTimerData
{
    AudioMessage audio_msg;

    RateLimitTimerData_Audio ()
        : RateLimitTimerData (t_Audio)
    {}

    ~RateLimitTimerData_Audio ()
    {
        page_pool->msgUnref (audio_msg.page_list.first);
    }
};

struct RtspService::RateLimitTimerData_Video : public RateLimitTimerData
{
    VideoMessage video_msg;

    RateLimitTimerData_Video ()
        : RateLimitTimerData (t_Video)
    {}

    ~RateLimitTimerData_Video ()
    {
        page_pool->msgUnref (video_msg.page_list.first);
    }
};

void
RtspService::rateLimitTimerTick (void * const _data)
{
    RateLimitTimerData * const data = static_cast <RateLimitTimerData*> (_data);
    RtspSession * const session = data->session;

    logD (rtsp_service, _funcp (session));

    switch (data->type) {
        case RateLimitTimerData::t_Audio: {
            RateLimitTimerData_Audio * const data_audio = static_cast <RateLimitTimerData_Audio*> (data);
            stream_doAudioMessage (&data_audio->audio_msg, session);
        } break;
        case RateLimitTimerData::t_Video: {
            RateLimitTimerData_Video * const data_video = static_cast <RateLimitTimerData_Video*> (data);
            stream_doVideoMessage (&data_video->video_msg, session);
        } break;
    }

    assert (session->media_source->type == MediaSource::Type::VodSource);
    VodSource * const vod_source = static_cast <VodSource*> (session->media_source.ptr());
    // TODO if we're not explicitly PAUSEd
    vod_source->resume ();
}

mt_sync_domain (vod_source_frontend) bool
RtspService::setRateLimitTimerIfNeeded (BoundSessions * const mt_nonnull bound_sessions,
                                        RtspSession   * const mt_nonnull session,
                                        MediaMessage  * const mt_nonnull msg)
{
    logD (rtsp_service, _funcp (bound_sessions, session));

    if (!bound_sessions->got_first_dts) {
        logD (rtsp_service, _funcp (bound_sessions) "!got_fist_dts");

        bound_sessions->first_dts_nanosec = msg->dts_nanosec;
        bound_sessions->got_first_dts = true;
        return false;
    }

    Uint64 timeout_microsec = 0;
    {
        Uint64 const time_microsec = getTimeMicroseconds ();

        logD (rtsp_service, _funcp (bound_sessions) "play_start_time_microsec ", bound_sessions->play_start_time_microsec);
        logD (rtsp_service, _funcp (bound_sessions) "           time_microsec ", time_microsec);

        Uint64 const relative_time_microsec = time_microsec - bound_sessions->play_start_time_microsec;
        Uint64 const relative_dts_microsec  = (msg->dts_nanosec - bound_sessions->first_dts_nanosec) / 1000;

        if (relative_time_microsec >= relative_dts_microsec) {
            logD (rtsp_service, _funcp (bound_sessions) "on time or lagging: time ", relative_time_microsec, ", dts ", relative_dts_microsec);

            if (relative_time_microsec - relative_dts_microsec >= MOMENT_RTSP__RATELIMIT_THRESHOLD_MICROSEC) {
                logD (rtsp_service, _funcp (bound_sessions) "lagging beyond threshold: ", relative_time_microsec - relative_dts_microsec);
                bound_sessions->first_dts_nanosec -= (relative_time_microsec - relative_dts_microsec) * 1000;
            }

            return false;
        }

        if (relative_dts_microsec - relative_time_microsec >= MOMENT_RTSP__RATELIMIT_THRESHOLD_MICROSEC) {
            logD (rtsp_service, _funcp (bound_sessions) "sleep time exceeds threshold");
            bound_sessions->first_dts_nanosec += (relative_dts_microsec - relative_time_microsec) * 1000;
            return false;
        }

        timeout_microsec = relative_dts_microsec - relative_time_microsec;
        logD (rtsp_service, _funcp (bound_sessions) "sleeping ", timeout_microsec, " microsec");
    }

    // FIXME session->media_source is treated as mt_const, but it's not.
    assert (session->media_source->type == MediaSource::Type::VodSource);
    VodSource * const vod_source = static_cast <VodSource*> (session->media_source.ptr());

    // TODO It would be more efficient if VodSource was informing us about the next frame's timestamp.
    //      It already has that info since it has to choose between audio and video frames on every
    //      read iteration. We could say "wait for X before reading the next frame" then. This would
    //      also solve the Timers ptr API leak. A major part of rate limiting probably belongs to
    //      VodSource implementations.

    vod_source->block ();

    Ref<RateLimitTimerData> data;
    switch (msg->msg_type) {
        case MediaMessage::Type_Audio: {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);
            data = grab (static_cast <RateLimitTimerData*> (new (std::nothrow) RateLimitTimerData_Audio));
            RateLimitTimerData_Audio * const data_audio = static_cast <RateLimitTimerData_Audio*> (data.ptr());
            data_audio->page_pool = audio_msg->page_pool;
            data_audio->audio_msg = *audio_msg;
            audio_msg->page_pool->msgRef (data_audio->audio_msg.page_list.first);
        } break;
        case MediaMessage::Type_Video: {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
            data = grab (static_cast <RateLimitTimerData*> (new (std::nothrow) RateLimitTimerData_Video));
            RateLimitTimerData_Video * const data_video = static_cast <RateLimitTimerData_Video*> (data.ptr());
            data_video->page_pool = video_msg->page_pool;
            data_video->video_msg = *video_msg;
            video_msg->page_pool->msgRef (data_video->video_msg.page_list.first);
        } break;
        default:
            unreachable ();
    }
    data->session = session;

    vod_source->getTimers()->addTimer_microseconds (
            CbDesc<Timers::TimerCallback> (rateLimitTimerTick, data, session, data),
            timeout_microsec,
            false /* periodical */,
            false /* auto_delete */, // The overhead of auto_delete is likely
                                     // too high for frequently set short-lived
                                     // oneshot timers.
            true /* delete_after_tick */);

    return true;
}

mt_sync_domain (vod_source_frontend) void
RtspService::vod_mediaMessage (MediaMessage * const mt_nonnull msg,
                               void         * const _bound_sessions)
{
    BoundSessions * const bound_sessions = static_cast <BoundSessions*> (_bound_sessions);

    logS (rtsp_service, _func, "bound_sessions 0x", fmt_hex, (UintPtr) bound_sessions, fmt_def, *msg);

    switch (msg->msg_type) {
        case MediaMessage::Type_Audio: {
            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);

            if (mt_unlikely (!audio_msg->frame_type.isAudioData()))
                return;

            Ref<RtspSession> const session = bound_sessions->weak_audio_rtsp_session.getRef ();
            if (!session)
                return;

            if (!(session->audio_data_session && session->audio_data_session->tcp_interleaved)) {
                if (setRateLimitTimerIfNeeded (bound_sessions, session, audio_msg))
                    return;
            }

            stream_doAudioMessage (audio_msg, session);
        } break;
        case MediaMessage::Type_Video: {
            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);

            // TODO Are these checks necessary for both audio and video?
            //      Think about cdata messages.
            //
            // if (mt_unlikely (!video_msg->frame_type.isVideoData()))
            //     return;

            Ref<RtspSession> const session = bound_sessions->weak_video_rtsp_session.getRef ();
            if (!session)
                return;

            if (!(session->video_data_session && session->video_data_session->tcp_interleaved)) {
                if (setRateLimitTimerIfNeeded (bound_sessions, session, video_msg))
                    return;
            }

            stream_doVideoMessage (video_msg, session);
        } break;
        case MediaMessage::Type_MediaDesc: {
            // TODO Handle new codec data
        } break;
        case MediaMessage::Type_None: {
            // No-op
        } break;
    }
}

mt_sync_domain (vod_source_frontend) void
RtspService::vod_endOfSource (VodSource::EndReason   const end_reason,
                              void                 * const _bound_sessions)
{
    BoundSessions * const bound_sessions = static_cast <BoundSessions*> (_bound_sessions);

    logD (rtsp_service, _func, "bound_sessions 0x", fmt_hex, (UintPtr) bound_sessions, ": "
          "end_reason ", end_reason);

  // TODO
}

void
RtspService::sendBadRequest (RtspConnection * const mt_nonnull rtsp_conn,
                             Uint32           const cseq)
{
    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 400 Bad Request\r\n"
            "CSeq: ", cseq, "\r\n"
            "\r\n");
}

void
RtspService::sendNotFound (RtspConnection * const mt_nonnull rtsp_conn,
                           Uint32           const cseq)
{
    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 404 Not Found\r\n"
            "CSeq: ", cseq, "\r\n"
            "\r\n");
}

void
RtspService::sendSessionNotFound (RtspConnection * const mt_nonnull rtsp_conn,
                                  Uint32           const cseq)
{
    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 454 Session Not Found\r\n"
            "CSeq: ", cseq, "\r\n"
            "\r\n");
}

void
RtspService::sendUnsupportedTransport (RtspConnection * const mt_nonnull rtsp_conn,
                                       Uint32           const cseq)
{
    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 461 Unsupported Transport\r\n"
            "CSeq: ", cseq, "\r\n"
// TEST
//            "Transport: RTP/AVP/TCP;interleaved=0-1\r\n"
            "\r\n");
}

void
RtspService::sendNotImplemented (RtspConnection * const mt_nonnull rtsp_conn,
                                 Uint32           const cseq)
{
    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 501 Not Implemented\r\n"
            "CSeq: ", cseq, "\r\n"
            "\r\n");
}

HttpServer::Frontend const RtspService::http_frontend = {
    rtspRawData,
    rtspRequest,
    NULL /* messageBody */,
    rtspClosed
};

void
RtspService::rtspRawData (Memory   const mem,
                          Size   * const mt_nonnull ret_accepted,
                          bool   * const mt_nonnull ret_req_next,
                          bool   * const mt_nonnull /* ret_block_input */,
                          void   * const _rtsp_conn)
{
    RtspConnection * const rtsp_conn = static_cast <RtspConnection*> (_rtsp_conn);

    *ret_req_next = false;
    *ret_accepted = 0;

    Byte const *buf = mem.mem();
    Size len = mem.len ();
    Size accepted = 0;
    for (;;) {
        switch (rtsp_conn->data_state) {
            case DataState_Header: {
                if (len >= 1 && buf [0] != 0x24 /* '$' */) {
                    *ret_accepted = accepted;
                    *ret_req_next = true;
                    return;
                }

                if (len < 4) {
                    *ret_accepted = accepted;
                    return;
                }

                rtsp_conn->got_len = 0;
                rtsp_conn->msg_len = ((Size) buf [2] << 8) + (Size) buf [3];

                logD (rtsp_service, _func, "interleaved data on channel ", (Uint32) buf [1]);

                len -= 4;
                buf += 4;
                accepted += 4;

                rtsp_conn->data_state = DataState_Data;
            } break;
            case DataState_Data: {
                assert (rtsp_conn->msg_len >= rtsp_conn->got_len);

                Size const left = rtsp_conn->msg_len - rtsp_conn->got_len;
                if (len < left) {
                    rtsp_conn->got_len += len;
                    *ret_accepted = mem.len();
                    return;
                }

                // We ignore all interleaved data.

                len -= left;
                buf += left;
                accepted += left;

                rtsp_conn->data_state = DataState_Header;
            } break;
        }
    }

    *ret_accepted = accepted;
}


// _________________________________ DESCRIBE __________________________________

struct RtspService::RtspRequest_Describe_Data : public Object
{
    StateMutex mutex;

    mt_const RtspService             *rtsp_service;
    mt_const WeakRef<RtspConnection>  weak_rtsp_conn;

    mt_const IpAddress     client_addr;
    mt_const StRef<String> request_line;
    mt_const StRef<String> stream_name;

    mt_const Uint32 cseq;

    mt_mutex (mutex) Ref<ClientManager::ClientSession> cm_session;
    mt_mutex (mutex) Ref<MediaSource> media_source;

    RtspRequest_Describe_Data (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

void
RtspService::rtspRequest_Describe (HttpRequest    * const mt_nonnull req,
                                   RtspConnection * const mt_nonnull rtsp_conn,
                                   Uint32           const cseq,
                                   bool           * const mt_nonnull ret_block_input)
{
    logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ": "
          "DESCRIBE");

    *ret_block_input = false;

    // DEBUG
    for (Count i = 0; i < req->getNumPathElems(); ++i)
        logD (rtsp_service, _this_func, "path #", i, ": \"", req->getPath (i), "\"");

    StRef<String> stream_name;
    if (req->getNumPathElems() > 0)
        stream_name = req->fillFilePath (0 /* first_idx */, req->getNumPathElems());
    else
        stream_name = newString (ConstMemory());

    logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ": "
          "stream_name: \"", stream_name, "\"");

// Deprecated
//    StRef<String> const stream_name =
//            newString (req->getNumPathElems() ? req->getPath (req->getNumPathElems() - 1) : ConstMemory());

    {
        Ref<RtspRequest_Describe_Data> const data = grabNewObject <RtspRequest_Describe_Data> ();
        data->rtsp_service   = this;
        data->weak_rtsp_conn = rtsp_conn;
        data->client_addr    = req->getClientAddress();
        data->request_line   = newString (req->getRequestLine());
        data->stream_name    = newString (stream_name);
        data->cseq           = cseq;

        ClientManager::ClientRequestInfo req_info;
        req_info.stream_link   = req->getFullPath();
        req_info.stream_name   = stream_name;
        req_info.client_addr   = req->getClientAddress();
        req_info.module_name   = "mod_rtsp";
        req_info.protocol_name = "rtsp";
        req_info.auth_id       = ConstMemory(); // TODO auth id
        req_info.rec_mode      = RecordingMode::NoRecording;

        mutex.lock ();

        rtsp_conn->got_start_time = false;
        {
            ConstMemory const start_time_mem = req->getParameter ("t");
            if (start_time_mem.len()) {
                if (!strToUint64 (start_time_mem, &rtsp_conn->start_time_millisec, NULL /* ret_endptr */, 10)) {
                    rtsp_conn->start_time_millisec = 0;
                } else {
                    rtsp_conn->got_start_time = true;
                }
            }
        }

        rtsp_conn->got_duration = false;
        {
            ConstMemory const duration_mem = req->getParameter ("d");
            if (duration_mem.len()) {
                if (!strToUint64 (duration_mem, &rtsp_conn->duration_millisec, NULL /* ret_endptr */, 10)) {
                    rtsp_conn->duration_millisec = 0;
                } else {
                    rtsp_conn->got_duration = true;
                }
            }
        }

        logD (rtsp_service, _this_func,
              "got_start_time ", rtsp_conn->got_start_time, ", "
              "start_time_millisec ", rtsp_conn->start_time_millisec);
        logD (rtsp_service, _this_func,
              "got_duration ", rtsp_conn->got_duration, ", "
              "duration_millisec ", rtsp_conn->duration_millisec);

        Ref<Object> const cm_session_guard = grabNewObject <Object> ();
        rtsp_conn->cm_session_guard = cm_session_guard;

        mutex.unlock ();

        Ref<MediaSource> media_source;
        Ref<ClientManager::ClientSession> cm_session;
        bool const complete =
                moment->getClientManager()->startWatching (
                        &req_info,
                        true /* enable_vod */,
                        // TODO Implement ClientSessionBackend::breakSession.
                        CbDesc<ClientManager::ClientSessionBackend> (),
                        cm_session_guard,
                        CbDesc<ClientManager::StartWatchingCallback> (
                                rtspRequest_Describe_startWatchingCallback, data, this, data),
                        &media_source,
                        &cm_session);
        if (!complete) {
            *ret_block_input = true;
            return;
        }

        if (!rtspRequest_Describe_completeStartWatching (data, rtsp_conn, media_source, cm_session)) {
            *ret_block_input = true;
            return;
        }
    }
}

void
RtspService::rtspRequest_Describe_startWatchingCallback (MediaSource                  * const media_source,
                                                         ClientManager::ClientSession * const mt_nonnull cm_session,
                                                         void                         * const _data)
{
    RtspRequest_Describe_Data * const data = static_cast <RtspRequest_Describe_Data*> (_data);
    RtspService * const self = data->rtsp_service;

    Ref<RtspConnection> const rtsp_conn = data->weak_rtsp_conn.getRef ();
    if (!rtsp_conn) {
        logD (rtsp_service, _self_func, "rtsp_conn gone");
        self->moment->getClientManager()->breakSession (cm_session);
        return;
    }

    if (self->rtspRequest_Describe_completeStartWatching (data, rtsp_conn, media_source, cm_session))
        rtsp_conn->tcp_block->http_server->unblockInput ();
}

bool
RtspService::rtspRequest_Describe_completeStartWatching (RtspRequest_Describe_Data    * const mt_nonnull data,
                                                         RtspConnection               * const mt_nonnull rtsp_conn,
                                                         MediaSource                  * const media_source,
                                                         ClientManager::ClientSession * const mt_nonnull cm_session)
{
    data->mutex.lock ();
    data->cm_session = cm_session;
    data->media_source = media_source;
    data->mutex.unlock ();

    if (!media_source) {
        logD (rtsp_service, _this_func, "null media_source");
        rtspRequest_Describe_finishNotFound (data, rtsp_conn, cm_session, "not found");
        return false;
    }

    switch ((MediaSource::Type::Value) media_source->type) {
        case MediaSource::Type::MediaStream: {
            MediaStream * const stream = static_cast <MediaStream*> (media_source);
            return rtspRequest_Describe_stream_completeStartWatching (data, rtsp_conn, stream, cm_session);
        } break;
        case MediaSource::Type::VodSource: {
            VodSource * const vod_source = static_cast <VodSource*> (media_source);
            return rtspRequest_Describe_vod_completeStartWatching (data, rtsp_conn, vod_source, cm_session);
        } break;
    }

    rtspRequest_Describe_finishNotFound (data, rtsp_conn, cm_session,
                                         makeString ("unsupported media source type", media_source->type));
    return true;
}

namespace {
    struct AudioInfo
    {
        StRef<String> aac_config;
        Uint32 audio_rate;
        Uint32 audio_channels;

        AudioInfo ()
            : audio_rate     (44100),
              audio_channels (1)
        {}
    };
}

static Result getAudioInfo_MediaDesc (MediaDesc_Audio * const mt_nonnull media_desc__audio,
                                      AudioInfo       * const mt_nonnull audio_info)
{
    if (media_desc__audio->getType() != MediaDesc_Audio::AAC) {
        logD (rtsp_service, _func, "no AAC audio");
        return Result::Failure;
    }
    MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (media_desc__audio);

    audio_info->audio_rate     = media_desc__audio->rate;
    audio_info->audio_channels = media_desc__audio->channels;

    ConstMemory const codec_data = media_desc__aac->getCodecData();
    audio_info->aac_config = newString (codec_data.len() * 2);

    for (Size i = 0; i < codec_data.len(); ++i) {
        Byte buf [3];
        {
            Format fmt (16 /* num_base */, 2 /* min_digits */, (unsigned) -1 /* precision */);
            makeStringInto (Memory::forObject (buf), fmt, (Byte) codec_data.mem() [i]);
        }
        memcpy (audio_info->aac_config->mem().buf() + i * 2, buf, 2);
    }

    return Result::Success;
}

// TODO Get rid of MediaDesc_Audio/AacSequenceHeader redundancy.
//
mt_mutex (stream->mutex) static Result getAudioInfo (MediaStream * const mt_nonnull stream,
                                                     AudioInfo   * const mt_nonnull audio_info)
{
    FrameSaver * const frame_saver = stream->getFrameSaver();

    if (MediaDesc_Audio * const media_desc__audio = frame_saver->getMediaDesc_Audio()) {
        logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
              "got MediaDesc_Audio");

        if (!getAudioInfo_MediaDesc (media_desc__audio, audio_info)) {
            logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
                  "getAudioInfo_MediaDesc() failed");
            return Result::Failure;
        }
    } else
    if (AudioMessage * const aac_seq_hdr = frame_saver->getAacSequenceHeader ()) {
        logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
              "no MediaDesc_Audio");

        audio_info->audio_rate     = aac_seq_hdr->rate;
        audio_info->audio_channels = aac_seq_hdr->channels;

        Size arr_len = aac_seq_hdr->msg_len;
        PagePool::PageListArray arr (aac_seq_hdr->page_list.first, aac_seq_hdr->msg_offset, arr_len);

        audio_info->aac_config = newString (arr_len * 2);

        for (Size i = 0; i < arr_len ; ++i) {
            Byte byte;
            arr.get (i, Memory::forObject (byte));

            Byte buf [3];
            {
                Format fmt (16 /* num_base */, 2 /* min_digits */, (unsigned) -1 /* precision */);
                makeStringInto (Memory::forObject (buf), fmt, byte);
            }
            memcpy (audio_info->aac_config->mem().buf() + i * 2, buf, 2);
        }
    } else {
        logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
              "no MediaDesc_Audio, no AAC codec data");
        return Result::Failure;
    }

    return Result::Success;
}

namespace {
    struct VideoInfo
    {
        Uint32 profile_level_id;
        StRef<String> sprop_parameter_sets;

        VideoInfo ()
            : profile_level_id (0)
        {}
    };
}

static Result encodeAvcCodecParameter (Array         * const mt_nonnull arr,
                                       Size            const arr_len,
                                       Size          * const mt_nonnull pos,
                                       StRef<String> * const mt_nonnull sprop_parameter_sets)
{
    if (arr_len < *pos + 2) {
        return Result::Failure;
    }
    Byte param_len_buf [2];
    arr->get (*pos, Memory::forObject (param_len_buf));
    *pos += 2;
    Uint16 const param_len = ((Uint16) param_len_buf [0] << 8) |
                             ((Uint16) param_len_buf [1] << 0);

    if (arr_len < *pos + param_len) {
        return Result::Failure;
    }
    Byte param_buf [param_len];
    arr->get (*pos, Memory (param_buf, param_len));
    *pos += param_len;

    {
        Byte * const encoded_param_buf = new (std::nothrow) Byte [base64EncodedBufferSize (param_len)];
        assert (encoded_param_buf);
        Size const encoded_param_len = base64Encode (ConstMemory (param_buf, param_len), encoded_param_buf);
        ConstMemory const encoded_param_mem (encoded_param_buf, encoded_param_len);

        if (*sprop_parameter_sets)
            *sprop_parameter_sets = makeString (*sprop_parameter_sets, ",", encoded_param_mem);
        else
            *sprop_parameter_sets = makeString (encoded_param_mem);

        delete[] encoded_param_buf;
    }

    return Result::Success;
}

static Result processAvcCodecData (Array     * const mt_nonnull arr,
                                   Size        const arr_len,
                                   VideoInfo * const mt_nonnull video_info)
{
    bool bad_data = false;
    do {
        if (arr_len < 6) {
            bad_data = true;
            break;
        }
        Byte block [6];
        arr->get (0, Memory::forObject (block));
        video_info->profile_level_id = ((Uint32) block [1] << 16) |
                                       ((Uint32) block [2] <<  8) |
                                       ((Uint32) block [3] <<  0);

        Uint32 const num_sps = block [5] & 0x1f;
        Size pos = 6;
        for (Size i = 0; i < num_sps; ++i) {
            if (!encodeAvcCodecParameter (arr, arr_len, &pos, &video_info->sprop_parameter_sets)) {
                bad_data = true;
                break;
            }
        }

        if (bad_data)
            break;

        if (arr_len < pos + 1) {
            bad_data = true;
            break;
        }
        Byte num_pps_buf [1];
        arr->get (pos, Memory::forObject (num_pps_buf));
        Uint32 const num_pps = num_pps_buf [0];
        logD (rtsp_service, _func, "num_sps ", num_sps, ", num_pps ", num_pps);
        ++pos;
        for (Size i = 0; i < num_pps; ++i) {
            if (!encodeAvcCodecParameter (arr, arr_len, &pos, &video_info->sprop_parameter_sets)) {
                bad_data = true;
                break;
            }
        }

        if (bad_data)
            break;
    } while (0);

    if (bad_data) {
        logD (rtsp_service, _func, "bad avc codec data");
        return Result::Failure;
    }

    return Result::Success;
}

static Result getVideoInfo_MediaDesc (MediaDesc_Video * const mt_nonnull media_desc__video,
                                      VideoInfo       * const mt_nonnull video_info)
{
    if (media_desc__video->getType() != MediaDesc_Video::AVC) {
        logD (rtsp_service, _func, "no AVC video");
        return Result::Failure;
    }
    MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (media_desc__video);

    ConstMemory const avc_cdata = media_desc__avc->getCodecData();
    ConstMemoryArray arr (avc_cdata);

    if (!processAvcCodecData (&arr, avc_cdata.len(), video_info)) {
        logD (rtsp_service, _func, "processAvcCodecData() failed (MediaDesc_AVC)");
        return Result::Failure;
    }

    return Result::Success;
}

// TODO Get rid of MediaDesc_Video/AvcSequenceHeader redundancy.
//
mt_mutex (stream->mutex) static Result getVideoInfo (MediaStream * const mt_nonnull stream,
                                                     VideoInfo   * const mt_nonnull video_info)
{
    FrameSaver * const frame_saver = stream->getFrameSaver();

    if (MediaDesc_Video * const media_desc__video = frame_saver->getMediaDesc_Video()) {
        logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
              "got MediaDesc_Video");

        if (!getVideoInfo_MediaDesc (media_desc__video, video_info)) {
            logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
                  "getVideoInfo_MediaDesc() failed");
            return Result::Failure;
        }
    } else
    if (VideoMessage * const avc_seq_hdr = frame_saver->getAvcSequenceHeader()) {
        logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
              "no MediaDesc_Video");

        Size const arr_len = avc_seq_hdr->msg_len;
        PagePool::PageListArray arr (avc_seq_hdr->page_list.first, avc_seq_hdr->msg_offset, arr_len);

        if (!processAvcCodecData (&arr, arr_len, video_info)) {
            logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream,
                  "processAvcCodecData() failed (AvcSequenceHeader)");
            return Result::Failure;
        }
    } else {
        logD (rtsp_service, _func, "stream 0x", fmt_hex, (UintPtr) stream, ": "
              "no MediaDesc_Video, no AVC codec data");
        return Result::Failure;
    }

    return Result::Success;
}

static StRef<String> makeSDP (IpAddress   const local_addr,
                              AudioInfo * const mt_nonnull audio_info,
                              VideoInfo * const mt_nonnull video_info,
                              bool no_audio,
                              bool no_video)
{
    Format fmt__profile_level_id;
    fmt__profile_level_id.num_base = 16;
    fmt__profile_level_id.min_digits = 6;

    return makeString (
            "v=0\r\n"
            "o=- 12345 1 IN IP4 ", IpAddress_NoPort (local_addr), "\r\n"
            "s=Streamed by Moment Video Server http://momentvideo.org\r\n"
            "i=live\r\n"
            "t=0 0\t\n"
            "a=tool:Moment Video Server\r\n"
            "a=type:broadcast\r\n"
            "a=control:*\r\n"
            "a=range:npt=0-\r\n",
            no_video ?
                    ConstMemory()
                  : makeString (
                            "m=video 0 RTP/AVP 96\r\n"
                            "c=IN IP4 ", IpAddress_NoPort (local_addr), "\r\n"
                            "b=AS:500\r\n"
                            // See http://stackoverflow.com/questions/3937241/reduce-video-buffering
                            // on reducing video opening delay.
                            // "b=AS:2\r\n"
                            "a=rtpmap:96 H264/90000\r\n"
                            "a=fmtp:96 packetization-mode=1;profile-level-id=", fmt__profile_level_id, video_info->profile_level_id, ";"
                                    "sprop-parameter-sets=", video_info->sprop_parameter_sets, "\r\n"
                            "a=control:track1\r\n")->mem(),
            no_audio ?
                    ConstMemory()
                  : makeString (
                            "m=audio 0 RTP/AVP 97\r\n"
                            "c=IN IP4 ", IpAddress_NoPort (local_addr), "\r\n"
                            "b=AS:96\r\n"
                            // See above (same as for video)
                            // "b=AS:2\r\n"
                            "a=rtpmap:97 MPEG4-GENERIC/", audio_info->audio_rate, "/", audio_info->audio_channels, "\r\n"
                            // Note: profile-level-id should probably be 0 or 0xfe (unknown/not specified).
                            "a=fmtp:97 streamtype=5;profile-level-id=1;mode=AAC-hbr;"
                                    "sizelength=13;indexlength=3;indexdeltalength=3;config=", audio_info->aac_config, "\r\n"
                            "a=control:track2\r\n")->mem()
            );
}

bool
RtspService::rtspRequest_Describe_stream_completeStartWatching (RtspRequest_Describe_Data    * const mt_nonnull data,
                                                                RtspConnection               * const mt_nonnull rtsp_conn,
                                                                MediaStream                  * const mt_nonnull stream,
                                                                ClientManager::ClientSession * const mt_nonnull cm_session)
{
  // TODO Check if the stream has a valid non-tentative PresentationDesc.
  //      If it doesn't, then wait for one with a configurable timeout.

    stream->lock ();

    bool no_audio = stream->getParam ("no_audio").len();
    bool no_video = stream->getParam ("no_video").len();
    logD (rtsp_service, _this_func,
          "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ", "
          "stream 0x", fmt_hex, (UintPtr) stream, ": "
          "no_audio: ", no_audio, ", no_video: ", no_video);

    AudioInfo audio_info;
    if (!no_audio) {
        if (!getAudioInfo (stream, &audio_info)) {
            logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn,
                  "getAudioInfo() failed");
            no_audio = true;
        }
    }

    VideoInfo video_info;
    if (!no_video) {
        if (!getVideoInfo (stream, &video_info)) {
            logD (rtsp_service, _this_func, "rts_conn 0x", fmt_hex, (UintPtr) rtsp_conn,
                  "getVideoInfo() failed");
            no_video = true;
        }
    }

    if (no_audio && no_video) {
        stream->unlock ();
        logD (rtsp_service, _func, "no audio, no video");
        rtspRequest_Describe_finishNotFound (data, rtsp_conn, cm_session, "no media");
        return true;
    }

    stream->unlock ();

    StRef<String> const sdp_str = makeSDP (rtsp_conn->local_addr,
                                           &audio_info,
                                           &video_info,
                                           no_audio,
                                           no_video);
    rtspRequest_Describe_finishOK (data,
                                   rtsp_conn,
                                   String::mem (sdp_str),
                                   audio_info.audio_rate);
    return true;
}

bool
RtspService::rtspRequest_Describe_vod_completeStartWatching (RtspRequest_Describe_Data    * const mt_nonnull data,
                                                             RtspConnection               * const mt_nonnull rtsp_conn,
                                                             VodSource                    * const mt_nonnull vod_source,
                                                             ClientManager::ClientSession * const mt_nonnull cm_session)
{
    VodSourceInfo source_info;
    {
        bool const complete =
                vod_source->querySourceInfo (
                        CbDesc<VodSource::QuerySourceInfoCallback> (
                                rtspRequest_Describe_vod_querySourceInfoCallback, data, this, data),
                        &source_info);
        if (!complete)
            return false;
    }

    return rtspRequest_Describe_vod_completeQuerySourceInfo (data,
                                                             rtsp_conn,
                                                             cm_session,
                                                             &source_info);
}

void
RtspService::rtspRequest_Describe_vod_querySourceInfoCallback (VodSourceInfo * const mt_nonnull source_info,
                                                               void          * const _data)
{
    RtspRequest_Describe_Data * const data = static_cast <RtspRequest_Describe_Data*> (_data);
    RtspService * const self = data->rtsp_service;

    data->mutex.lock ();
    ClientManager::ClientSession * const cm_session = data->cm_session;
    data->mutex.unlock ();

    Ref<RtspConnection> const rtsp_conn = data->weak_rtsp_conn.getRef ();
    if (!rtsp_conn) {
        logD (rtsp_service, _self_func, "rtsp_conn gone");
        self->moment->getClientManager()->breakSession (cm_session);
        return;
    }

    if (self->rtspRequest_Describe_vod_completeQuerySourceInfo (data, rtsp_conn, cm_session, source_info))
        rtsp_conn->tcp_block->http_server->unblockInput ();
}

bool
RtspService::rtspRequest_Describe_vod_completeQuerySourceInfo (RtspRequest_Describe_Data    * const mt_nonnull data,
                                                               RtspConnection               * const mt_nonnull rtsp_conn,
                                                               ClientManager::ClientSession * const mt_nonnull cm_session,
                                                               VodSourceInfo                * const mt_nonnull source_info)
{
    bool no_audio = true;
    AudioInfo audio_info;
    if (source_info->media_desc__audio) {
        if (!getAudioInfo_MediaDesc (source_info->media_desc__audio, &audio_info)) {
            logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ": "
                  "getAudioInfo_MediaDesc() failed");
        } else {
            no_audio = false;
        }
    }

    bool no_video = true;
    VideoInfo video_info;
    if (source_info->media_desc__video) {
        if (!getVideoInfo_MediaDesc (source_info->media_desc__video, &video_info)) {
            logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ": "
                  "getVideoInfo_MediaDesc() failed");
        } else {
            no_video = false;
        }
    }

    if (no_audio && no_video) {
        logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ": "
              "no audio, no video");
        rtspRequest_Describe_finishNotFound (data, rtsp_conn, cm_session, "no media");
        return true;
    }

    StRef<String> const sdp_str = makeSDP (rtsp_conn->local_addr,
                                           &audio_info,
                                           &video_info,
                                           no_audio,
                                           no_video);
    rtspRequest_Describe_finishOK (data,
                                   rtsp_conn,
                                   String::mem (sdp_str),
                                   audio_info.audio_rate);
    return true;
}

void
RtspService::rtspRequest_Describe_finishOK (RtspRequest_Describe_Data    * const mt_nonnull data,
                                            RtspConnection               * const mt_nonnull rtsp_conn,
                                            ConstMemory                    const sdp_mem,
                                            Uint32                         const audio_rate)
{
    mutex.lock ();
    rtsp_conn->stream_name  = newString (data->stream_name);
    rtsp_conn->audio_rate   = audio_rate;
    {
        data->mutex.lock ();
        rtsp_conn->media_source = data->media_source;
        data->mutex.unlock ();
    }
    mutex.unlock ();

    Format fmt__profile_level_id;
    fmt__profile_level_id.num_base = 16;
    fmt__profile_level_id.min_digits = 6;

    logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn,
          "reply SDP:\n", sdp_mem);

    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 200 OK\r\n"
            "CSeq: ", data->cseq, "\r\n"
            "Content-Type: application/sdp\r\n"
            "Content-Length: ", sdp_mem.len(), "\r\n"
            "\r\n",
            sdp_mem);

    logA (rtsp_service, "rtsp OK ", data->client_addr, " ", data->request_line);
}

void
RtspService::rtspRequest_Describe_finishNotFound (RtspRequest_Describe_Data    * const mt_nonnull data,
                                                  RtspConnection               * const mt_nonnull rtsp_conn,
                                                  ClientManager::ClientSession * const mt_nonnull cm_session,
                                                  ConstMemory                    const err_str)
{
    sendNotFound (rtsp_conn, data->cseq);
    logA (rtsp_service, "rtsp 404 ", data->client_addr, " ", data->request_line, " [", err_str, "]");

    moment->getClientManager()->breakSession (cm_session);
}

// _____________________________________________________________________________


void
RtspService::rtspRequest_Setup (HttpRequest    * const mt_nonnull req,
                                RtspConnection * const mt_nonnull rtsp_conn,
                                RtspSession    * const mt_nonnull _session,
                                Uint32           const cseq,
                                Time             const cur_time_millisec)
{
    logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ": "
          "SETUP");

    Ref<RtspSession> session = _session;

    bool setup_audio;
    {
        ConstMemory track_name;
        {
          // Trying to work around VLC's broken URI generation.
          // It generates URIs of the form rtsp://1.2.3.4/foo?a=10/track1

            ConstMemory mem = req->getParams();
            logD (rtsp_service, _this_func, "params: \"", mem, "\"");

            Size pos = mem.len();
            for (; pos > 0; --pos) {
                if (mem.buf() [pos - 1] == '/')
                    break;
            }

            if (pos < mem.len()) {
                mem = mem.region (pos, mem.len() - pos);
                if (   equal (mem, "track1")
                    || equal (mem, "track2"))
                {
                    track_name = mem;
                }
            }
        }

        if (!track_name.len())
            track_name = req->getNumPathElems() ? req->getPath (req->getNumPathElems() - 1) : ConstMemory();

        if (equal (track_name, "track1")) {
            setup_audio = false;
        } else
        if (equal (track_name, "track2")) {
            setup_audio = true;
        } else {
            logD (rtsp_service, _func, "wrong track: ", track_name);
            sendBadRequest (rtsp_conn, cseq);
            return;
        }
    }

    RtspTransportHeaderInfo transport_info;
    parseRtspTransportHeader (req, &transport_info);

    if (!transport_info.is_tcp_interleaved && tcp_only) {
        logD (rtsp_service, _func, "TCP only, replying 461 Unsupported Transport");
        sendUnsupportedTransport (rtsp_conn, cseq);
        return;
    }

    if (!transport_info.is_tcp_interleaved) {
        if (!transport_info.got_client_port) {
            logD (rtsp_service, _func, "could not parse client_port transport parameter");
            sendBadRequest (rtsp_conn, cseq);
            return;
        }
    } else {
        if (!enable_tcp) {
            logD (rtsp_service, _func, "TCP disabled, replying 461 Unsupported Transport");
            sendUnsupportedTransport (rtsp_conn, cseq);
            return;
        }
    }

//#error TODO SSID = session id (Uint32 part);
//#error TODO Use RTCP RR as valid requests, based on src ip/port + SSID for session identification
//#error TODO Use ReceiverMapping

    Ref<RtpDataSession> const data_session = grabNewObject <RtpDataSession> ();
//    #warning TODO Use random SSRC.
    data_session->ssrc = setup_audio ? 0x11223344 : 0x55667788;
    data_session->tcp_interleaved = transport_info.is_tcp_interleaved;
//    #warning TODO Use random sequence number.
    data_session->sequence_number = 1000;
    data_session->got_keyframe = false;

    if (!transport_info.is_tcp_interleaved) {
        // We use a single pair of UDP sockets to serve all clients.
        data_session->rtp_server_sockets = rtp_server_sockets;
        data_session->rtp_port  = transport_info.client_port_rtp;
        data_session->rtcp_port = transport_info.client_port_rtcp;
        setIpAddress (rtsp_conn->client_addr.ip_addr, transport_info.client_port_rtp,  &data_session->rtp_addr);
        setIpAddress (rtsp_conn->client_addr.ip_addr, transport_info.client_port_rtcp, &data_session->rtcp_addr);

// TODO            session->rtp_sender->setFrontend ();
        data_session->rtp_sender->setConnection (data_session->rtp_server_sockets->rtp_socket);
        data_session->rtp_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());

// TODO            session->rtcp_sender->setFrontend ();
        data_session->rtcp_sender->setConnection (data_session->rtp_server_sockets->rtcp_socket);
        data_session->rtcp_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());

        data_session->rtp_sender->setDatagramSendtoAddress (data_session->rtp_addr);
        logD (rtsp_service, _func, "RTP sendto_addr: ", data_session->rtp_addr);
        data_session->rtcp_sender->setDatagramSendtoAddress (data_session->rtcp_addr);
        logD (rtsp_service, _func, "RTCP sendto_addr: ", data_session->rtcp_addr);
    } else {
        data_session->tcp_rtp_channel  = transport_info.tcp_channel_rtp;
        data_session->tcp_rtcp_channel = transport_info.tcp_channel_rtcp;

        data_session->tcp_block = rtsp_conn->tcp_block;
    }

    mutex.lock ();

    data_session->freq = setup_audio ? (rtsp_conn->audio_rate / 100) : 900;

    if (session) {
        if (!session->valid) {
            mutex.unlock ();
            logD (rtsp_service, _func, "session destroyed");
            sendSessionNotFound (rtsp_conn, cseq);
            return;
        }

        if (session->playing) {
            mutex.unlock ();
            logD (rtsp_service, _func, "SETUP after PLAY is not allowed");
            sendBadRequest (rtsp_conn, cseq);
            return;
        }

        if (setup_audio) {
            if (rtsp_conn->bound_sessions->weak_audio_rtsp_session.isValid()) {
                mutex.unlock ();
                logD (rtsp_service, _func, "double audio SETUP request");
                sendBadRequest (rtsp_conn, cseq);
                return;
            }

            assert (!session->audio_data_session);
            session->audio_data_session = data_session;

            logD (rtsp_service, _this_func, "bound_sessions ", _ptr (rtsp_conn->bound_sessions), ": setting weak_audio_rtsp_session to ", _ptr (session));
            rtsp_conn->bound_sessions->weak_audio_rtsp_session = session;
        } else {
            if (rtsp_conn->bound_sessions->weak_video_rtsp_session.isValid()) {
                mutex.unlock ();
                logD (rtsp_service, _func, "double video SETUP request");
                sendBadRequest (rtsp_conn, cseq);
                return;
            }

            assert (!session->video_data_session);
            session->video_data_session = data_session;

            logD (rtsp_service, _this_func, "bound_sessions ", _ptr (rtsp_conn->bound_sessions), ": setting weak_video_rtsp_session to ", _ptr (session));
            rtsp_conn->bound_sessions->weak_video_rtsp_session = session;
        }
    } else {
        session = grabNewObject <RtspSession> ();
        session->weak_rtsp_service = this;
        session->weak_rtsp_conn = rtsp_conn;
        session->session_id = makeString (fmt_hex, randomUint32());
        session->bound_sessions = rtsp_conn->bound_sessions;
        session->last_request_time_millisec = cur_time_millisec;

        session->got_timestamp_offset = false;
        session->timestamp_offset = 0;
        session->last_timestamp = 0;

        if (setup_audio) {
            session->audio_data_session = data_session;

            rtsp_conn->audio_rtsp_session_id = newString (session->session_id->mem());
            logD (rtsp_service, _this_func, "bound_sessions ", _ptr (rtsp_conn->bound_sessions), ": setting weak_audio_rtsp_session to ", _ptr (session));
            rtsp_conn->bound_sessions->weak_audio_rtsp_session = session;
        } else {
            session->video_data_session = data_session;

            rtsp_conn->video_rtsp_session_id = newString (session->session_id->mem());
            logD (rtsp_service, _this_func, "bound_sessions ", _ptr (rtsp_conn->bound_sessions), ": setting weak_video_rtsp_session to ", _ptr (session));
            rtsp_conn->bound_sessions->weak_video_rtsp_session = session;
        }

        session->session_list_el = session_list.append (session);
        rtsp_session_cleanup_list.append (session);

      #ifdef MOMENT_STATE_COMMERCIAL_OR_EVALUATION
        {
          // protection
            if (   ! MOMENT_STATE_COMMERCIAL_OR_EVALUATION(A)
                && ! MOMENT_FEATURES_RTSP_RTMP_A
                && ! MOMENT_FEATURES_RTSP_A)
            {
                thread_ctx->getTimers()->addTimer_microseconds (
                        CbDesc<Timers::TimerCallback> (stream_closed, session, session),
                        2 * 60 * 1000000 /* time_microseconds */,
                        false /* periodical */,
                        true  /* auto_delete */,
                        true  /* delete_after_tick */);
            }
        }
      #endif

        if (rtsp_conn->media_source
            && rtsp_conn->media_source->type == MediaSource::Type::VodSource
            && !session->bound_sessions->subscribed)
        {
            session->bound_sessions->subscribed = true;

            VodSource * const vod_source = static_cast <VodSource*> (rtsp_conn->media_source.ptr());
            vod_source->setFrontend (
                    CbDesc<VodSource::Frontend> (
                            &vod_source_frontend, session->bound_sessions, session->bound_sessions));
        }
    }

    if (session->cm_session_guard
        && session->cm_session_guard.ptr() != rtsp_conn->cm_session_guard.ptr())
    {
        logW (rtsp_service, _this_func,
              "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ", "
              "session 0x",   fmt_hex, (UintPtr) session.ptr(), ": "
              "ClientManager session mismatch");
    }
    session->cm_session_guard = rtsp_conn->cm_session_guard;

    mutex.unlock ();

    StRef<String> transport_str;
    if (!transport_info.is_tcp_interleaved) {
        transport_str = makeString ("Transport: RTP/AVP;unicast;destination=",
                                    IpAddress_NoPort (rtsp_conn->client_addr), ";"
                                    "source=", IpAddress_NoPort (rtsp_conn->local_addr), ";"
                                    "server_port=", server_rtp_port, "-", server_rtcp_port, "\r\n");
    } else {
        transport_str = makeString ("Transport: RTP/AVP/TCP;"
                                    "interleaved=",
                                            transport_info.tcp_channel_rtp, "-",
                                            transport_info.tcp_channel_rtcp,
                                    "\r\n");
    }

    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 200 OK\r\n"
            "CSeq: ", cseq, "\r\n",
            transport_str,
            "Session: ", session->session_id, /* ";timeout=", session_request_timeout_millisec / 1000, */ "\r\n"
            "\r\n");
}


// ___________________________________ PLAY ____________________________________

void
RtspService::rtspRequest_Play (HttpRequest    * const mt_nonnull req,
                               RtspConnection * const mt_nonnull rtsp_conn,
                               RtspSession    * const mt_nonnull session,
                               Uint32           const cseq)
{
    logD (rtsp_service, _this_func, "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ": "
          "PLAY");

#if 0
//    #warning TODO Rule out /track1 and /track2 suffixes - probably just get rid of that check
    ConstMemory const stream_name =
            req->getNumPathElems() ? req->getPath (req->getNumPathElems() - 1) : ConstMemory();
#endif

    mutex.lock ();

    if (!session->valid) {
        mutex.unlock ();
        logD (rtsp_service, _func, "session destroyed");
        sendSessionNotFound (rtsp_conn, cseq);
        return;
    }

#if 0
// Questionable and potentially problematic.
//
    if (!rtsp_conn->stream_name || !equal (rtsp_conn->stream_name->mem(), stream_name)) {
        mutex.unlock ();
        logD (rtsp_service, _func, "stream name mismatch");
        sendBadRequest (rtsp_conn, cseq);
        return;
    }
#endif

    bool disable_udp_data = false;
    if (udp_silence
        && (!session->audio_data_session || !session->audio_data_session->tcp_interleaved)
        && (!session->video_data_session || !session->video_data_session->tcp_interleaved))
    {
        disable_udp_data = true;
    }

    if (!rtsp_conn->media_source) {
        mutex.unlock ();
        logD (rtsp_service, _func, "null stream, DESCRIBE was not called");
        sendBadRequest (rtsp_conn, cseq);
        return;
    }

    if (session->playing) {
        mutex.unlock ();
        logD (rtsp_service, _func, "already playing");
        rtsp_conn->tcp_block->sender->send (
                page_pool,
                true /* do_flush */,
                "RTSP/1.0 200 OK\r\n"
                "CSeq: ", cseq, "\r\n"
                "Session: ", session->session_id, /* ";timeout=", session_request_timeout_millisec / 1000, */ "\r\n"
                "\r\n");
        return;
    }
    session->playing = true;

    session->bound_sessions->play_start_time_microsec = getTimeMicroseconds ();

    mt_unlocks (mutex)
      switch ((MediaSource::Type::Value) rtsp_conn->media_source->type) {
          case MediaSource::Type::MediaStream: {
              MediaStream * const stream = static_cast <MediaStream*> (rtsp_conn->media_source.ptr());
              if (! mt_unlocks (mutex) rtspRequest_Play_handleMediaStream (req, rtsp_conn, session, cseq, stream, disable_udp_data)) {
                  logD (rtsp_service, _this_func,
                        "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ", "
                        "session 0x",   fmt_hex, (UintPtr) session, ": "
                        "rtspRequest_Play_handleMediaStream() failed");
                  return;
              }
          } break;
          case MediaSource::Type::VodSource: {
              VodSource * const vod_source = static_cast <VodSource*> (rtsp_conn->media_source.ptr());
              if (! mt_unlocks (mutex) rtspRequest_Play_handleVodSource (req, rtsp_conn, session, cseq, vod_source, disable_udp_data)) {
                  logD (rtsp_service, _this_func,
                        "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ", "
                        "session 0x",   fmt_hex, (UintPtr) session, ": "
                        "rtspRequest_Play_handleVodSource() failed");
                  return;
              }
          } break;
          default: {
              mutex.unlock ();
              logE (rtsp_service, _this_func,
                    "rtsp_conn 0x", fmt_hex, (UintPtr) rtsp_conn, ", "
                    "session 0x",   fmt_hex, (UintPtr) session, ": "
                    "unknown MediaSource type: ", rtsp_conn->media_source->type);
              sendNotFound (rtsp_conn, cseq);
              return;
          } break;
      }

    StRef<String> rtp_info;
    {
        StRef<String> video_rtp_info;
        if (session->video_data_session)
            // TODO Use random seq and rtptime
            video_rtp_info = makeString ("url=", req->getFullPath(), "/track1;seq=1000;rtptime=1000000");

        StRef<String> audio_rtp_info;
        if (session->audio_data_session)
            // TODO Use random seq and rtptime
            audio_rtp_info = makeString ("url=", req->getFullPath(), "/track2;seq=1000;rtptime=1000000");

        if (video_rtp_info || audio_rtp_info) {
            rtp_info = makeString ("RTP-Info: ",
                                   (video_rtp_info ? video_rtp_info->mem() : ConstMemory()),
                                   ((video_rtp_info && audio_rtp_info) ? ConstMemory (",") : ConstMemory()),
                                   (audio_rtp_info ? audio_rtp_info->mem() : ConstMemory()),
                                   "\r\n");
        }
    }

    rtsp_conn->tcp_block->sender->send (
            page_pool,
            true /* do_flush */,
            "RTSP/1.0 200 OK\r\n"
            "CSeq: ", cseq, "\r\n"
            "Range: npt=0.000-\r\n"
            "Session: ", session->session_id, /* ";timeout=", session_request_timeout_millisec / 1000, */ "\r\n",
            rtp_info ? rtp_info->mem() : ConstMemory(),
            "\r\n");

    if (!disable_udp_data) {
      /* With these initial reports, VLC resets its buffering at least one extra time (annoying).
       *
        if (session->audio_data_session) {
            logD (rtsp_service, _func, "sending audio RTCP SR");
            sendRtcpSenderReport (session->audio_data_session);
        }

        if (session->video_data_session) {
            logD (rtsp_service, _func, "sending video RTCP SR");
            sendRtcpSenderReport (session->video_data_session);
        }
       */

        if (   (session->audio_data_session
                && (   session->audio_data_session->tcp_interleaved
                    || session->audio_data_session->rtcp_port != 0))
            || (session->video_data_session
                && (   session->video_data_session->tcp_interleaved
                    || session->video_data_session->rtcp_port != 0)))
        {
            thread_ctx->getTimers()->addTimer_microseconds (
                    CbDesc<Timers::TimerCallback> (senderReportTick, session, session),
                    1000000 /* 1 second */,
                    true    /* periodical */,
                    true    /* auto_delete */,
                    false   /* delete_after_tick */);
        }
    }
}

mt_unlocks (mutex) Result
RtspService::rtspRequest_Play_handleMediaStream (HttpRequest    * const mt_nonnull /* req */,
                                                 RtspConnection * const mt_nonnull rtsp_conn,
                                                 RtspSession    * const mt_nonnull session,
                                                 Uint32           const cseq,
                                                 MediaStream    * const mt_nonnull _stream,
                                                 bool             const disable_udp_data)
{
    Ref<VideoStream> const stream = _stream;
    session->media_source = stream;
    bool const subscribed = session->bound_sessions->subscribed;
    session->bound_sessions->subscribed = true;
    mutex.unlock ();

    stream->plusOneWatcher ();

    mutex.lock ();
    if (!session->valid) {
        mutex.unlock ();
        stream->minusOneWatcher ();

        logD (rtsp_service, _func, "session destroyed");
        sendSessionNotFound (rtsp_conn, cseq);
        return Result::Failure;
    }

    stream->lock ();
    if (stream->isClosed_locked()) {
        stream->unlock ();
        mutex.unlock ();

        logD (rtsp_service, _func, "stream closed");
        sendNotFound (rtsp_conn, cseq);
        return Result::Failure;
    }

    if (!subscribed && !disable_udp_data) {
        session->stream_sbn =
                stream->getEventInformer()->subscribe_locked (
                        CbDesc<VideoStream::Events> (&stream_events, session->bound_sessions, session->bound_sessions));
    }
    stream->unlock ();

    mutex.unlock ();
    return Result::Success;
}

mt_unlocks (mutex) Result
RtspService::rtspRequest_Play_handleVodSource (HttpRequest    * const mt_nonnull /* req */,
                                               RtspConnection * const mt_nonnull rtsp_conn,
                                               RtspSession    * const mt_nonnull session,
                                               Uint32           const /* cseq */,
                                               VodSource      * const mt_nonnull _vod_source,
                                               bool             const disable_udp_data)
{
    Ref<VodSource> const vod_source = _vod_source;
    session->media_source = vod_source;

    if (!disable_udp_data) {
        if (rtsp_conn->got_start_time)
            vod_source->seek (rtsp_conn->start_time_millisec * 1000);

        if (rtsp_conn->got_duration) {
            vod_source->start (true /* stop_time_enabled */,
                               1000 * (  rtsp_conn->start_time_millisec
                                       + rtsp_conn->duration_millisec));
        } else {
            vod_source->start (false /* stop_time_enabled */,
                               0     /* stop_time_microsec */);
        }
    }

    mutex.unlock ();
    return Result::Success;
}

// _____________________________________________________________________________


void
RtspService::rtspRequest (HttpRequest * const mt_nonnull req,
                          bool        * const mt_nonnull ret_block_input,
                          void        * const _rtsp_conn)
{
    RtspConnection * const rtsp_conn = static_cast <RtspConnection*> (_rtsp_conn);
    Ref<RtspService> const self = rtsp_conn->weak_rtsp_service.getRef ();
    if (!self)
        return;

    logD (rtsp_service, _func, "req: ", req->getRequestLine());

    Time const cur_time_millisec = getTimeMilliseconds();

    self->mutex.lock ();
    rtsp_conn->last_request_time_millisec = cur_time_millisec;
    self->rtsp_conn_cleanup_list.remove (rtsp_conn);
    self->rtsp_conn_cleanup_list.append (rtsp_conn);
    self->mutex.unlock ();

    Uint32 cseq = 0;
    if (!strToUint32 (req->getHeader ("cseq"), &cseq, NULL, 10))
        logD (rtsp_service, _func, "no CSeq");

    ConstMemory const session_id = req->getHeader ("session");
    Ref<RtspSession> session;
    if (rtsp_conn->audio_rtsp_session_id
        && equal (rtsp_conn->audio_rtsp_session_id->mem(), session_id))
    {
        session = rtsp_conn->bound_sessions->weak_audio_rtsp_session.getRef ();
        if (!session) {
            logD (rtsp_service, _func, "session gone");
            self->sendSessionNotFound (rtsp_conn, cseq);
            return;
        }
    } else
    if (rtsp_conn->video_rtsp_session_id
        && equal (rtsp_conn->video_rtsp_session_id->mem(), session_id))
    {
        session = rtsp_conn->bound_sessions->weak_video_rtsp_session.getRef ();
        if (!session) {
            logD (rtsp_service, _func, "session gone");
            self->sendSessionNotFound (rtsp_conn, cseq);
            return;
        }
    } else {
        if (session_id.buf()) {
            logD (rtsp_service, _func, "session not found: ", session_id);
            self->sendSessionNotFound (rtsp_conn, cseq);
            return;
        }
    }

    if (session) {
        self->mutex.lock ();
        if (session->valid) {
            session->last_request_time_millisec = cur_time_millisec;
            self->rtsp_session_cleanup_list.remove (session);
            self->rtsp_session_cleanup_list.append (session);
        }
        self->mutex.unlock ();
    }

    if (equal (req->getMethod(), "OPTIONS")) {
        logD (rtsp_service, _func, "OPTIONS");

        rtsp_conn->tcp_block->sender->send (
                self->page_pool,
                true /* do_flush */,
                "RTSP/1.0 200 OK\r\n"
                "CSeq: ", cseq, "\r\n"
                "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, "
                        "PAUSE, GET_PARAMETER, SET_PARAMETER\r\n"
                "\r\n");
    } else
    if (equal (req->getMethod(), "DESCRIBE")) {
        self->rtspRequest_Describe (req, rtsp_conn, cseq, ret_block_input);
    } else
    if (equal (req->getMethod(), "SETUP")) {
        self->rtspRequest_Setup (req, rtsp_conn, session, cseq, cur_time_millisec);
    } else
    if (   equal (req->getMethod(), "PLAY")
        || equal (req->getMethod(), "PAUSE")
        || equal (req->getMethod(), "GET_PARAMETER")
        || equal (req->getMethod(), "SET_PARAMETER")
        || equal (req->getMethod(), "TEARDOWN"))
    {
        logD (rtsp_service, _func, req->getMethod());

        if (!session) {
            logD (rtsp_service, _func, "session not specified");
            self->sendBadRequest (rtsp_conn, cseq);
            return;
        }

        if (!equal (req->getMethod(), "PLAY")) {
            if (equal (req->getMethod(), "TEARDOWN")) {
              // TODO Support a single TEARDOWN with a presentation URI to terminate both audio and video sessions.
                self->mutex.lock ();
                self->destroyRtspSession (session);
                self->mutex.unlock ();
            }

            rtsp_conn->tcp_block->sender->send (
                    self->page_pool,
                    true /* do_flush */,
                    "RTSP/1.0 200 OK\r\n"
                    "CSeq: ", cseq, "\r\n"
                    "Session: ", session->session_id, /* ";timeout=", self->session_request_timeout_millisec / 1000, */ "\r\n"
                    "\r\n");
            return;
        }

        self->rtspRequest_Play (req, rtsp_conn, session, cseq);
    } else {
        logD (rtsp_service, _func, "unknown method: ", req->getMethod());
        rtsp_conn->tcp_block->sender->send (
                self->page_pool,
                true /* do_flush */,
                "RTSP/1.0 200 OK\r\n",
                session ? makeString ("Session: ", session->session_id, /* ";timeout=", self->session_request_timeout_millisec / 1000, */ "\r\n")->mem() : ConstMemory(),
                "CSeq: ", cseq, "\r\n"
                "\r\n");
    }
}

void
RtspService::rtspClosed (HttpRequest * const /* req */,
                         Exception   * const exc_,
                         void        * const _rtsp_conn)
{
    RtspConnection * const rtsp_conn = static_cast <RtspConnection*> (_rtsp_conn);
    Ref<RtspService> const self = rtsp_conn->weak_rtsp_service.getRef ();
    if (!self)
        return;

    if (exc_)
        logD (rtsp_service, _func, "rtsp_conn ", fmt_hex, (UintPtr) rtsp_conn, ", exception: ", exc_->toString());
    else
        logD (rtsp_service, _func_, "rtsp_conn ", fmt_hex, (UintPtr) rtsp_conn);

    self->mutex.lock ();
    self->destroyRtspConnection (rtsp_conn);
    self->mutex.unlock ();
}

bool
RtspService::acceptOneConnection ()
{
    logD (rtsp_service, _this_func_);

    Ref<RtspConnection> const rtsp_conn = grabNewObject <RtspConnection> ();
    rtsp_conn->tcp_block = grabNewObject <TcpConnectionBlock> ();
    rtsp_conn->bound_sessions = grabNewObject <BoundSessions> ();

    IpAddress client_addr;
    {
        TcpServer::AcceptResult const res = tcp_server->accept (rtsp_conn->tcp_block->tcp_conn,
                                                                &client_addr);
        if (res == TcpServer::AcceptResult::Error) {
            logE (rtsp_service, _func, exc->toString());
            return false;
        }

        if (res == TcpServer::AcceptResult::NotAccepted)
            return false;

        assert (res == TcpServer::AcceptResult::Accepted);
    }

    logD (rtsp_service, _this_func, "bound_sessions ", _ptr (rtsp_conn->bound_sessions));

    // TODO Force local addr in config
    if (!rtsp_conn->tcp_block->tcp_conn->getLocalAddress (&rtsp_conn->local_addr)) {
        logW (rtsp_service, _func, "tcp_conn.getLocalAddress() failed: ", exc->toString());
        rtsp_conn->local_addr = IpAddress();
    }

    rtsp_conn->weak_rtsp_service = this;
    rtsp_conn->client_addr = client_addr;
    rtsp_conn->last_request_time_millisec = getTimeMilliseconds();

//#warning TODO Use feedback from TCP and UDP senders to block VOD sources and to drop frames from live streams.
    rtsp_conn->tcp_block->sender->init (thread_ctx->getDeferredProcessor());
    rtsp_conn->tcp_block->sender->setConnection (rtsp_conn->tcp_block->tcp_conn);
    rtsp_conn->tcp_block->receiver->init (rtsp_conn->tcp_block->tcp_conn,
                                          thread_ctx->getDeferredProcessor());

    rtsp_conn->tcp_block->http_server->init (
            CbDesc<HttpServer::Frontend> (&http_frontend, rtsp_conn, rtsp_conn),
            rtsp_conn->tcp_block->receiver,
            rtsp_conn->tcp_block->sender,
            thread_ctx->getDeferredProcessor(),
            page_pool,
            client_addr,
            false /* client_mode */,
            true  /* put_headers_to_hash */);

    mutex.lock ();
    rtsp_conn->pollable_key = thread_ctx->getPollGroup()->addPollable (rtsp_conn->tcp_block->tcp_conn->getPollable());
    if (!rtsp_conn->pollable_key) {
        mutex.unlock ();
        logE (rtsp_service, _func, "addPollable() failed: ", exc->toString());
        return true;
    }

    rtsp_conn->conn_list_el = conn_list.append (rtsp_conn);
    rtsp_conn_cleanup_list.append (rtsp_conn);
    mutex.unlock ();

    rtsp_conn->tcp_block->receiver->start ();
    return true;
}

TcpServer::Frontend const RtspService::tcp_server_frontend = {
    accepted
};

void
RtspService::accepted (void * const _self)
{
    RtspService * const self = static_cast <RtspService*> (_self);

    for (;;) {
        if (!self->acceptOneConnection ())
            break;
    }
}

mt_throws Result
RtspService::bind (IpAddress const addr)
{
    return tcp_server->bind (addr);
}

Result
RtspService::start ()
{
    if (!tcp_server->listen()) {
        logF (rtsp_service, _this_func, "tcp_server.listen() failed: ", exc->toString());
        return Result::Failure;
    }

    mutex.lock ();

    srv_pollable_key = thread_ctx->getPollGroup()->addPollable (tcp_server->getPollable());
    if (!srv_pollable_key) {
        mutex.unlock ();
        logF (rtsp_service, _this_func, "addPollable() failed (srv): ", exc->toString());
        return Result::Failure;
    }

    rtp_socket_pollable_key = thread_ctx->getPollGroup()->addPollable (rtp_server_sockets->rtp_socket->getPollable());
    if (!rtp_socket_pollable_key) {
        mutex.unlock ();
        logF (rtsp_service, _this_func, "addPollable() failed (rtp): ", exc->toString());
        return Result::Failure;
    }

    rtcp_socket_pollable_key = thread_ctx->getPollGroup()->addPollable (rtp_server_sockets->rtcp_socket->getPollable());
    if (!rtcp_socket_pollable_key) {
        mutex.unlock ();
        logF (rtsp_service, _this_func, "addPollable() failed (rtcp): ", exc->toString());
        return Result::Failure;
    }

    mutex.unlock ();

    if (!tcp_server->start ()) {
        logF (rtsp_service, _this_func, "tcp_server.start() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

mt_const Result
RtspService::init (MomentServer * const mt_nonnull moment,
                   PagePool     * const mt_nonnull page_pool,
                   bool           const enable_tcp,
                   bool           const tcp_only,
                   bool           const udp_silence,
                   Time           const timeout_check_interval_millisec,
                   Time           const session_request_timeout_millisec,
                   Time           const conn_request_timeout_millisec)
{
    this->moment      = moment;
    // TODO select thread context for each connection
    this->thread_ctx  = moment->getServerApp()->getServerContext()->getMainThreadContext();
    this->page_pool   = page_pool;
    this->enable_tcp  = enable_tcp;
    this->tcp_only    = tcp_only;
    this->udp_silence = udp_silence;
    this->session_request_timeout_millisec = session_request_timeout_millisec;
    this->conn_request_timeout_millisec    = conn_request_timeout_millisec;

    this->rtp_server_sockets = grabNewObject <RtpServerSockets> ();

    deferred_reg.setDeferredProcessor (thread_ctx->getDeferredProcessor());

    {
        RtpPortPair rtp_port_pair;
        if (!rtp_port_pair.openAndBindSocketPair (rtp_server_sockets->rtp_socket,
                                                  rtp_server_sockets->rtcp_socket,
                                                  &server_rtp_port,
                                                  &server_rtcp_port))
        {
            logE (rtsp_service, _func, "openAndBindSocketPair() failed");
            return Result::Failure;
        } else {
            logD (rtsp_service, _func, "server_rtp_port: ", server_rtp_port, ", server_rtcp_port: ", server_rtcp_port);
        }
    }

    if (!tcp_server->open ()) {
        logE (rtsp_service, _func, "tcp_server.open() failed");
        return Result::Failure;
    }

    tcp_server->init (CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, this),
                      thread_ctx->getDeferredProcessor(),
                      thread_ctx->getTimers());

    if (timeout_check_interval_millisec) {
        thread_ctx->getTimers()->addTimer_microseconds (
                CbDesc<Timers::TimerCallback> (timeoutCheckTick, this, this),
                timeout_check_interval_millisec * 1000,
                true  /* periodical */,
                true  /* auto_delete */,
                false /* delete_after_tick */);
    }

    return Result::Success;
}

RtspService::RtspService (EmbedContainer * const embed_container)
    : Object                     (embed_container),
      enable_tcp                 (true),
      tcp_only                   (false),
      udp_silence                (false),
      session_request_timeout_millisec (0),
      conn_request_timeout_millisec    (0),
      tcp_server                 (this /* embed_container */),
      server_rtp_port            (0),
      server_rtcp_port           (0)
{
}

RtspService::~RtspService ()
{
    mutex.lock ();

    if (srv_pollable_key) {
        thread_ctx->getPollGroup()->removePollable (srv_pollable_key);
        srv_pollable_key = NULL;
    }

    if (rtp_socket_pollable_key) {
        thread_ctx->getPollGroup()->removePollable (rtp_socket_pollable_key);
        rtp_socket_pollable_key = NULL;
    }

    if (rtcp_socket_pollable_key) {
        thread_ctx->getPollGroup()->removePollable (rtcp_socket_pollable_key);
        rtcp_socket_pollable_key = NULL;
    }

    {
        List< Ref<RtspSession> >::iterator iter (session_list);
        while (!iter.done()) {
            Ref<RtspSession> const &session = iter.next ()->data;
            destroyRtspSession (session);
        }
        assert (session_list.isEmpty());
    }

    {
        List< Ref<RtspConnection> >::iterator iter (conn_list);
        while (!iter.done()) {
            Ref<RtspConnection> const &rtsp_conn = iter.next ()->data;
            destroyRtspConnection (rtsp_conn);
        }
        assert (conn_list.isEmpty());
    }

    assert (rtsp_session_cleanup_list.isEmpty());
    assert (rtsp_conn_cleanup_list.isEmpty());

    assert (receiver_mapping_hash.isEmpty());

    mutex.unlock ();
}

}

