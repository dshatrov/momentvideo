/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cctype>

#include <moment/util_moment.h>
#include <moment/util_moment_http.h>

#include <moment-rtsp/util_rtsp.h>

#include <moment-rtsp/rtsp_client.h>


// TODO Send RTCP Receiver Report packets.

//#warning No audio in SDP -> mark MediaStream as "no_audio" (come up with a way to do that).
//#warning A caps mechanism is probably required as well (to know media type without waiting for actual data).


namespace MomentRtsp {

static LogGroup libMary_logGroup_frames ("mod_rtsp.frames", LogLevel::I);

StatCounter_AtomicInt stat_counter__rtsp_client (0, "rtsp_client", 0, "rtsp");

RtpPortPair RtspClient::audio_port_pair;
RtpPortPair RtspClient::video_port_pair;

mt_mutex (mutex) void
RtspClient::rtpConnectionStart (RtpConnection * const mt_nonnull rtp_conn)
{
    rtp_conn->rtp_receiver->start ();
    rtp_conn->rtcp_receiver->start ();
}

mt_mutex (mutex) void
RtspClient::doStart ()
{
    rtpConnectionStart (&rtp_audio);
    rtpConnectionStart (&rtp_video);

    conn_receiver->start ();
    nextStateAction ();
}

mt_mutex (mutex) void
RtspClient::rtpConnectionRelease (RtpConnection * const mt_nonnull rtp_conn)
{
    if (rtp_conn->rtp_pollable_key) {
        thread_ctx->getPollGroup()->removePollable (rtp_conn->rtp_pollable_key);
        rtp_conn->rtp_pollable_key = NULL;
    }

    if (rtp_conn->rtcp_pollable_key) {
        thread_ctx->getPollGroup()->removePollable (rtp_conn->rtcp_pollable_key);
        rtp_conn->rtcp_pollable_key = NULL;
    }
}

mt_unlocks (mutex) void
RtspClient::destroySelf (DisconnectReason const disconnect_reason)
{
    close_pending = false;

    if (closed) {
        mutex.unlock ();
        return;
    }
    closed = true;

    rtpConnectionRelease (&rtp_audio);
    rtpConnectionRelease (&rtp_video);

    if (pollable_key) {
        thread_ctx->getPollGroup()->removePollable (pollable_key);
        pollable_key = NULL;
    }

    page_pool->msgUnref (page_list.first);
    page_list.reset ();

    page_pool->msgUnref (video_fragmented_list.first);
    video_fragmented_list.reset ();
    video_fragmented_len = 0;

    mutex.unlock ();

    if (fetch_connection_frontend) {
        fetch_connection_frontend.call (fetch_connection_frontend->disconnected,
                                        disconnect_reason);
    }
}

mt_mutex (mutex) void
RtspClient::processRtpPacket_Audio (PagePool::PageListHead const &packet_pages,
                                    Size   const packet_len_without_padding,
                                    bool   const marker,
                                    Uint64 const timestamp)
{
    Byte * const beg_buf = packet_pages.first->getData();
    Size   const beg_len = packet_pages.first->data_len;

    if (beg_len < 12 + 2)
        return;

    Byte const * const payload = beg_buf + 12;
    Size const offs = payload - beg_buf;

  // RFC 3640 payload

    if (!marker) {
        // TODO support fragmentation
        logF_ (_this_func, "RFC3640 fragmentation is not supported");
        return;
    }

    assert (packet_len_without_padding >= offs);
    PagePool::PageListArray arr (packet_pages.first, offs, packet_len_without_padding - offs);
    Size pos = 0;

    Byte au_headers_length_mem [2];
    arr.get (pos, Memory::forObject (au_headers_length_mem));
    Uint32 au_headers_length = ((Uint32) au_headers_length_mem [0] << 8) |
                               ((Uint32) au_headers_length_mem [1] << 0);

    Uint32 const au_headers_bytes = (au_headers_length + 7) / 8;

    /* Unused?
    // AAC-hbr AU headers are 16 bits long.
    Size const num_au_headers = au_headers_length / 16;
    */

    if (2 + au_headers_bytes > packet_len_without_padding - offs) {
        logW_ (_this_func, "invalid audio packet");
        return;
    }

    {
        Byte byte;
        arr.get (2 + au_headers_bytes - 1, Memory::forObject (byte));
    }

    Size const au_len = packet_len_without_padding - 2 - au_headers_bytes - offs;

    // TODO This extra copying is completely excessive.
    PagePool::PageListHead tmp_pages;
    page_pool->getFillPagesFromPages (&tmp_pages,
                                      arr.getNextPageToAccess(),
                                      arr.getNextInPageOffset(),
                                      au_len);

    AudioMessage audio_msg;
    audio_msg.codec_id = AudioCodecId::AAC;
    audio_msg.rate = audio_clock_rate;
    audio_msg.channels = audio_channels;

    audio_msg.frame_type = AudioFrameType::RawData;
    // TODO proper timestamp conversion, synchronization
    audio_msg.pts_nanosec = (Uint64) timestamp * 100000 / audio_clock_rate * 10000 + 10000000000;
    audio_msg.dts_nanosec = audio_msg.pts_nanosec;

    audio_msg.page_pool = page_pool;
    audio_msg.page_list = tmp_pages;
    audio_msg.msg_len = au_len;
    audio_msg.msg_offset = 0;

    last_frame_time_millisec = getTimeMilliseconds();
    got_first_frame = true;

//#warning FIXME Firing with 'mutex' locked!
    stream->fireAudioMessage (&audio_msg);

    page_pool->msgUnref (tmp_pages.first);

#if 0
// Wrong LATM code (RFC 3016)

    if (!marker) {
//#warning TODO Support LATM fragmentation
        logF_ (_this_func, "LATM fragmentation is not supported");
        return;
    }

    assert (msg_len >= offs);
    PagePool::PageListArray arr (page_list.first, offs, msg_len - offs);
    Size pos = 0;

//#warning TODO numSubFrames as i_end
    for (Count i = 0; i < 1; ++i) {
        logD_ (_this_func, "aac iteration ", i);

        Size frame_len = 0;
        while (pos < msg_len) {
            Byte byte;
            arr.get (pos, Memory::forObject (byte));
            ++pos;

            frame_len += byte;
            if (byte != 0xff)
                break;
        }

        logD_ (_this_func, "frame_len ", frame_len);

        if (pos + frame_len > msg_len) {
            logD_ (_this_func, "bad aac packet, pos ", pos, ", "
                   "frame_len ", frame_len, ", msg_len ", msg_len);
            break;
        }

        PagePool::PageListHead tmp_pages;
        page_pool->getFillPagesFromPages (&tmp_pages,
                                          arr.getLastAccessedPage(),
                                          arr.getLastAccessedInPageOffset(),
                                          frame_len);

        AudioMessage audio_msg;
        audio_msg.codec_id = AudioCodecId::AAC;

        // TODO rate, etc.
//                audio_msg.rate = ;
//                audio_msg.channels = ;

        audio_msg.frame_type = AudioFrameType::RawData;
        // TODO proper timestamp conversion, synchronization
        audio_msg.pts_nanosec = (Uint64) timestamp * 10 / 9 * 10000;
        audio_msg.dts_nanosec = audio_msg.pts_nanosec;

        audio_msg.page_pool = page_pool;
        audio_msg.page_list = tmp_pages;
        audio_msg.msg_len = frame_len;
        audio_msg.msg_offset = 0;

        last_frame_time_millisec = getTimeMilliseconds();
        got_first_frame = true;

//#warning FIXME Firing with 'mutex' locked!
        stream->fireAudioMessage (&audio_msg);

        page_pool->msgUnref (tmp_pages.first);
    }
#endif
}

mt_mutex (mutex) void
RtspClient::processRtpPacket_Video (PagePool::PageListHead const &packet_pages,
                                    Size   const packet_len_without_padding,
                                    bool   const marker,
                                    Uint16 const sequence_number,
                                    Uint64 const timestamp)
{
    Size const payload_offs = 12;

    if (packet_len_without_padding < payload_offs + 1) {
        logS (frames, _this_func, "packet is too short: ", packet_len_without_padding, " bytes");
        return;
    }

    Size const payload_len = packet_len_without_padding - payload_offs;

    Byte const first_byte = PagePool::getMsgByte (packet_pages.first, payload_offs + 0);
    Byte const nal_unit_type = first_byte & 0x1f;

    if (nal_unit_type >= 1 && nal_unit_type <= 23) {
      // Single NAL unit packet
        logS (frames, _this_func, "single NAL, nal_unit_type ", nal_unit_type);

        Size const nal_len  = payload_len;
        Size const nal_offs = payload_offs;

        processNal (nal_unit_type,
                    &packet_pages,
                    nal_len,
                    nal_offs,
                    timestamp,
                    marker);
    } else
    if (nal_unit_type == 28) {
      // FU-A
        if (payload_len >= 2) {
            Byte const second_byte = PagePool::getMsgByte (packet_pages.first, payload_offs + 1);
            bool const start = second_byte & 0x80;
            bool const end   = second_byte & 0x40;
            unsigned sub_nal_type = second_byte & 0x1f;

            logS (frames, _this_func, "FU-A: start ", start, ", end ", end, ", sub_nal_type ", sub_nal_type);

#warning DEBUG
            if (sub_nal_type == 0) {
                if (logLevelOn (frames, LogLevel::S))
                    PagePool::logPages (LogLevel::S, packet_pages.first, 0 /* first_page_offs */, _this_func, "FU-A body: ");
            }

            if (start) {
                discard_video_fragment = false;

                page_pool->msgUnref (video_fragmented_list.first);
                video_fragmented_list.reset ();
                video_fragmented_len = 1;

                Byte hdr [1] = { 0 };
                page_pool->getFillPages (&video_fragmented_list, ConstMemory::forObject (hdr));
            } else {
                if (discard_video_fragment) {
                    logS (frames, _this_func, "discarding FU-A");
                    return;
                }

                if (sequence_number != expected_video_fragment_seq_num) {
                    logS (frames, _this_func, "sequence number gap, discarding FU-A");

                    discard_video_fragment = true;

                    page_pool->msgUnref (video_fragmented_list.first);
                    video_fragmented_list.reset ();
                    video_fragmented_len = 0;
                    return;
                }
            }
            expected_video_fragment_seq_num = sequence_number + 1;

            Size const offs = payload_offs + 2;
            assert (offs <= packet_len_without_padding);
            if (offs < packet_len_without_padding) {
                page_pool->getFillPagesFromPages (&video_fragmented_list,
                                                  packet_pages.first,
                                                  offs,
                                                  packet_len_without_padding - offs);
                video_fragmented_len += packet_len_without_padding - offs;
            }

            if (end) {
                // Discard fragments until the next FU-A start.
                discard_video_fragment = true;

                video_fragmented_list.first->getData() [0] = sub_nal_type | (first_byte & 0x60);

                processNal (sub_nal_type,
                            &video_fragmented_list,
                            video_fragmented_len,
                            0 /* nal_offs */,
                            timestamp,
                            marker);

                page_pool->msgUnref (video_fragmented_list.first);
                video_fragmented_list.reset ();
                video_fragmented_len = 0;
            }
        } // if got FU-A header
        else {
            logS (frames, _this_func, "no FU-A header");
        }
    } // if nal type
    else {
        logS (frames, _this_func, "unknown nal_unit_type ", nal_unit_type);
    }
}

mt_mutex (mutex) void
RtspClient::processRtpPacket (RtpConnection * const mt_nonnull rtp_conn)
{
    if (rtp_conn->is_audio) {
        if (state < State::SetupAudioSent)
            return;
    } else {
        if (state < State::SetupVideoSent)
            return;
    }

    Byte * const beg_buf = page_list.first->getData();
    Size   const beg_len = page_list.first->data_len;
    if (beg_len < 8)
        return;

    unsigned const version = (beg_buf [0] & 0xc0) >> 6;
    if (version != 2) {
        logD (frames, _this_func, "bad version: ", version);
        return;
    }

    bool     const padding         = beg_buf [0] & 0x20;
    bool     const extension       = beg_buf [0] & 0x10;
    unsigned const csrc_count      = beg_buf [0] & 0x0f;
    bool     const marker          = beg_buf [1] & 0x80;
    unsigned const payload_type    = beg_buf [1] & 0x7f;
    Uint16   const sequence_number = (beg_buf [2] << 8) | beg_buf [3];

    Uint64 timestamp =   ((Uint64) beg_buf [4] << 24)
                       | ((Uint64) beg_buf [5] << 16)
                       | ((Uint64) beg_buf [6] <<  8)
                       | ((Uint64) beg_buf [7] <<  0);

    Uint32 const ssrc = (beg_buf [8] << 24) | (beg_buf [9] << 16) | (beg_buf [10] << 8) | beg_buf [11];

    logS (frames, _this_func,
          "ver ", version, ", "
          "pad ", (padding ? "true " : "false"), ", "
          "ext ", extension, ", "
          "csrc_cnt ", csrc_count, ", "
          "mark ", (marker ? "true " : "false"), ", "
          "pt ", payload_type, ", "
          "seq ", sequence_number, ", "
          "ts ", fmt_hex, timestamp, ", "
          "ssrc ", fmt_hex, ssrc);

    if (!rtp_conn->available) {
        logS (frames, _this_func, "!available (", (rtp_conn->is_audio ? "audio" : "video"), ")");
        return;
    }

    if (rtp_conn->is_audio) {
        if (payload_type != audio_payload_type) {
            logS (frames, _this_func, "audio payload type mismatch");
            return;
        }
    } else {
        if (payload_type != video_payload_type) {
            logS (frames, _this_func, "video payload type mismatch");
            return;
        }
    }

    if (rtp_conn->got_expected_seq) {
        Uint64 const max = (1 << 16);

        Uint64 delta = 0;
        if (sequence_number >= rtp_conn->expected_seq)
            delta = sequence_number - rtp_conn->expected_seq;
        else
            delta = rtp_conn->expected_seq - sequence_number;

        if (delta > (max >> 1))
            delta = max - delta;

        // TODO Figure out the right delta
        if (delta > 3000) {
            logD_ (_this_func, "bad seq delta: ", sequence_number, ", exp ", rtp_conn->expected_seq);
            return;
        }
    }

    if (rtp_conn->got_expected_rtptime) {
        Uint64 const max = ((Uint64) 1 << 32);

        Uint64 delta = 0;
        if (timestamp >= rtp_conn->expected_rtptime)
            delta = timestamp - rtp_conn->expected_rtptime;
        else
            delta = rtp_conn->expected_rtptime - timestamp;

        if (delta > (max >> 1))
            delta = max - delta;

//#warning TODO audio timestamp units - ?
        // TODO Figure out the right delta
        if (delta > 90000 * 60) {
            logD_ (_this_func, "bad rtptime delta: ", timestamp, ", exp ", rtp_conn->expected_rtptime);
            // With some IP cameras, timestamps may jump spontaneously.
            // We have to adopt to timestamp skew instead of discarding packets.
            //
            // return;
            rtp_conn->initial_rtptime = rtp_conn->expected_rtptime - rtp_conn->initial_rtptime + timestamp;
            logD_ (_this_func, "new initial_rtptime: ", rtp_conn->initial_rtptime);
        }
    }

    rtp_conn->expected_seq = sequence_number;
    rtp_conn->got_expected_seq = true;

    rtp_conn->expected_rtptime = timestamp;
    if (rtp_conn->got_expected_rtptime) {
//#warning TODO audio timestamp units - ?
        timestamp -= (Uint64) rtp_conn->initial_rtptime /* - 900000 */;
    } else {
        rtp_conn->got_expected_rtptime = true;
        rtp_conn->initial_rtptime = timestamp;
        timestamp = /* 900000 */ 0;
    }

    Size msg_len_without_padding = msg_len;
    if (padding) {
        PagePool::Page * const last_page = page_list.last;
        assert (last_page && last_page->data_len >= 1);
        Byte const padding = last_page->getData() [last_page->data_len - 1];
        if (padding > msg_len) {
            logD_ (_this_func, "bad padding ", (Uint32) padding, ", msg_len ", msg_len);
            return;
        }
        msg_len_without_padding = msg_len - padding;
    }

    if (reordering_delay_microsec) {
        Time const cur_time_microsec = getTimeMicroseconds();

#warning TODO limit number of items/sizeof of data queued.
        {
            RtpPacket * const new_packet = new (std::nothrow) RtpPacket;
            assert (new_packet);

            new_packet->page_list = page_list;
            page_list.reset ();

            new_packet->packet_len                 = msg_len;
            new_packet->packet_len_without_padding = msg_len_without_padding;
            new_packet->rtp_seq_num                = sequence_number;
            new_packet->arrival_time_microsec      = cur_time_microsec;
            new_packet->marker                     = marker;
            new_packet->timestamp                  = timestamp;

            RtpPacket *old_packet = NULL;
            if (!rtp_conn->reordering_queue.isEmpty()) {
                old_packet = rtp_conn->reordering_queue.getLast();
                while (old_packet) {
                    Uint32 delta;
                    if (sequence_number >= old_packet->rtp_seq_num) {
                        delta = sequence_number - old_packet->rtp_seq_num;
                    } else {
                        delta = 65536 - (Uint32) old_packet->rtp_seq_num + (Uint32) sequence_number;
                    }

                    if (delta < 32768) {
                        rtp_conn->reordering_queue.append (new_packet, old_packet);
                        break;
                    }

                    logD_ (_this_func, "reordering ", sequence_number, " before ", old_packet->rtp_seq_num);
                    old_packet = RtpPacketList::getPrevious (old_packet);
                }
            }

            if (!old_packet)
                rtp_conn->reordering_queue.prepend (new_packet);
        }

        while (!rtp_conn->reordering_queue.isEmpty()) {
            RtpPacket * const rtp_packet = rtp_conn->reordering_queue.getFirst();
            if (   rtp_packet->arrival_time_microsec < cur_time_microsec
                && cur_time_microsec - rtp_packet->arrival_time_microsec >= reordering_delay_microsec)
            {
                logS (frames, _this_func, "dequeue seq ", rtp_packet->rtp_seq_num
                      , " csum ", calculateChecksumPages (rtp_packet->page_list.first, 0 /* msg_offset */), " "
                      " len ", rtp_packet->packet_len_without_padding);

                if (rtp_conn->is_audio) {
                    processRtpPacket_Audio (rtp_packet->page_list,
                                            rtp_packet->packet_len_without_padding,
                                            rtp_packet->marker,
                                            rtp_packet->timestamp);
                } else {
                    processRtpPacket_Video (rtp_packet->page_list,
                                            rtp_packet->packet_len_without_padding,
                                            rtp_packet->marker,
                                            rtp_packet->rtp_seq_num,
                                            rtp_packet->timestamp);
                }

                page_pool->msgUnref (rtp_packet->page_list.first, rtp_packet->packet_len, 0 /* msg_offset */);
                rtp_packet->page_list.reset ();
                rtp_conn->reordering_queue.remove (rtp_packet);
            } else {
                break;
            }
        }
    } else {
        if (rtp_conn->is_audio) {
            processRtpPacket_Audio (page_list,
                                    msg_len_without_padding,
                                    marker,
                                    timestamp);
        } else {
            processRtpPacket_Video (page_list,
                                    msg_len_without_padding,
                                    marker,
                                    sequence_number,
                                    timestamp);
        }
    }
}

Receiver::Frontend const RtspClient::audio_rtp_receiver_frontend = {
    audioRtpInput,
    audioRtpEof,
    audioRtpError,
    NULL /* processInputFrom */
};

Receiver::ProcessInputResult
RtspClient::audioRtpInput (Memory   const mem,
                           Size   * const ret_accepted,
                           void   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    self->mutex.lock ();
    if (self->closed) {
        self->mutex.unlock ();
        return Receiver::ProcessInputResult::Error;
    }

  // #warning TODO For UDP, there's no need in this extra copying: we have all the data in place immediately in a contiguous buffer.

    if (self->page_list.first) {
        self->page_pool->msgUnref (self->page_list.first);
        self->page_list.reset ();
    }

    self->page_pool->getFillPages (&self->page_list, mem);
    self->msg_len = mem.len();

    self->processRtpPacket (&self->rtp_audio);

    self->mutex.unlock ();

    *ret_accepted = mem.len();
    return Receiver::ProcessInputResult::Normal;
}

void
RtspClient::audioRtpEof (Memory   const /* unprocessed_mem */,
                         void   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);
    logD_ (_self_func_);
}

void
RtspClient::audioRtpError (Exception * const exc_,
                           Memory      const /* unprocessed_mem */,
                           void      * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    if (exc_)
        logD_ (_self_func, exc_->toString());
    else
        logD_ (_self_func_);
}

Receiver::Frontend const RtspClient::audio_rtcp_receiver_frontend = {
    audioRtcpInput,
    audioRtcpEof,
    audioRtcpError,
    NULL /* processInputFrom */
};

Receiver::ProcessInputResult
RtspClient::audioRtcpInput (Memory const mem,
                            Size * const ret_accepted,
                            void * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);
    logS_ (_self_func, "len ", mem.len());

    *ret_accepted = mem.len();

    return Receiver::ProcessInputResult::Normal;
}

void
RtspClient::audioRtcpEof (Memory   const /* unprocessed_mem */,
                          void   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);
    logD_ (_self_func_);
}

void
RtspClient::audioRtcpError (Exception * const exc_,
                            Memory      const /* unprocessed_mem */,
                            void      * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    if (exc_)
        logD_ (_self_func, exc_->toString());
    else
        logD_ (_self_func_);
}

Receiver::Frontend const RtspClient::video_rtp_receiver_frontend = {
    videoRtpInput,
    videoRtpEof,
    videoRtpError,
    NULL /* processInputFrom */
};

Receiver::ProcessInputResult
RtspClient::videoRtpInput (Memory   const mem,
                           Size   * const ret_accepted,
                           void   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    self->mutex.lock ();
    if (self->closed) {
        self->mutex.unlock ();
        return Receiver::ProcessInputResult::Error;
    }

  // #warning TODO For UDP, there's no need in this extra copying: we have all the data in place immediately in a contiguous buffer.

    if (self->page_list.first) {
        self->page_pool->msgUnref (self->page_list.first);
        self->page_list.reset ();
    }

    self->page_pool->getFillPages (&self->page_list, mem);
    self->msg_len = mem.len();

    self->processRtpPacket (&self->rtp_video);

    self->mutex.unlock ();

    *ret_accepted = mem.len();
    return Receiver::ProcessInputResult::Normal;
}

void
RtspClient::videoRtpEof (Memory   const /* unprocessed_mem */,
                         void   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);
    logD_ (_self_func_);
}

void
RtspClient::videoRtpError (Exception * const exc_,
                           Memory      const /* unprocessed_mem */,
                           void      * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    if (exc_)
        logD_ (_self_func, exc_->toString());
    else
        logD_ (_self_func_);
}

Receiver::Frontend const RtspClient::video_rtcp_receiver_frontend = {
    videoRtcpInput,
    videoRtcpEof,
    videoRtcpError,
    NULL /* processInputFrom */
};

Receiver::ProcessInputResult
RtspClient::videoRtcpInput (Memory const mem,
                            Size * const ret_accepted,
                            void * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);
    logS_ (_self_func, "len ", mem.len());

    *ret_accepted = mem.len();

    return Receiver::ProcessInputResult::Normal;
}

void
RtspClient::videoRtcpEof (Memory   const /* unprocessed_mem */,
                          void   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);
    logD_ (_self_func_);
}

void
RtspClient::videoRtcpError (Exception * const exc_,
                            Memory      const /* unprocessed_mem */,
                            void      * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    if (exc_)
        logD_ (_self_func, exc_->toString());
    else
        logD_ (_self_func_);
}

TcpConnection::Frontend const RtspClient::tcp_conn_frontend = {
    connected
};

void
RtspClient::connected (Exception * const exc_,
                       void      * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    logD_ (_self_func_);

    if (exc_) {
        logE_ (_self_func, "exception: ", exc_->toString());
        mt_unlocks (mutex) self->destroySelf (DisconnectReason::CouldNotEstablishUnderlyingConnection);
        return;
    }

    self->mutex.lock ();
    self->doStart ();
    self->mutex.unlock ();
}

HttpServer::Frontend const RtspClient::http_server_frontend = {
    rtspRawData,
    rtspRequest,
    rtspMessageBody,
    rtspClosed
};

void
RtspClient::rtspRawData (Memory   const mem,
                         Size   * const mt_nonnull ret_accepted,
                         bool   * const mt_nonnull ret_req_next,
                         bool   * const mt_nonnull /* ret_block_input */,
                         void   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    logD_ (_self_func_, "mem len ", mem.len());

    *ret_req_next = false;
    *ret_accepted = 0;

    Byte const *buf = mem.mem();
    Size len = mem.len();
    Size accepted = 0;
    for (;;) {
        switch (self->data_state.val()) {
            case DataState::Header: {
                if (len >= 1 && buf [0] != 0x24) {
                    *ret_accepted = accepted;
                    *ret_req_next = true;
                    logD_ (_self_func, "no '$', accepted: ", accepted);
                    return;
                }

                if (len < 4) {
                    *ret_accepted = accepted;
                    logD_ (_self_func, "incomplete header, accepted: ", accepted);
                    return;
                }

                self->channel_id = buf [1];

                self->got_len = 0;
                self->msg_len = ((Size) buf [2] << 8) + (Size) buf [3];

                logD_ (_self_func, "interleaved data on channel ", self->channel_id, " len ", self->msg_len);

                len -= 4;
                buf += 4;
                accepted += 4;

                self->data_state = DataState::Data;
            } break;
            case DataState::Data: {
                assert (self->msg_len >= self->got_len);

                Size const left = self->msg_len - self->got_len;
                if (len < left) {
                    self->got_len += len;

                    if (self->channel_id == 0 || self->channel_id == 2) {
                        self->mutex.lock ();
                        if (!self->closed) {
                            self->page_pool->getFillPages (&self->page_list, ConstMemory (buf, len));
                        }
                        self->mutex.unlock ();
                    }

                    *ret_accepted = mem.len();
                    logD_ (_func, "incomplete data, len ", len, ", got_len ", self->got_len, ", accepted ", *ret_accepted);
                    return;
                }

                if (self->channel_id == 0 || self->channel_id == 2) {
                    self->mutex.lock ();
                    if (!self->closed) {
                        self->page_pool->getFillPages (&self->page_list, ConstMemory (buf, left));

                        if (self->page_list.first)
                            self->processRtpPacket (self->channel_id == 0 ? &self->rtp_audio : &self->rtp_video);

                        self->page_pool->msgUnref (self->page_list.first);
                        self->page_list.reset ();
                    }
                    self->mutex.unlock ();
                }

                len -= left;
                buf += left;
                accepted += left;

                self->data_state = DataState::Header;
            } break;
        }
    } // for (;;)

    unreachable ();

//    *ret_accepted = accepted;
}

mt_mutex (mutex) void
RtspClient::processNal (Byte const nal_type,
                        PagePool::PageListHead const * const mt_nonnull nal_pages,
                        Size const nal_len,
                        Size const nal_offs,
                        Time const timestamp,
                        bool const marker)
{
//#warning TODO Set forbidden_zero_bit to zero or discard data if it's non-zero (better).

    // iOS doesn't play some streams with heavy SEI usage.
    if (drop_sei_nal_units && nal_type == 6)
        return;

    if (nal_type != 0) {
        h264_parser.parseNal (nal_pages->first,
                              nal_offs,
                              nal_len,
                              timestamp /* pts */,
                              timestamp /* dts */,
                              marker    /* last_in_access_unit */);
    } else {
        // nal_type == 0 => the payload is h.264 Annex B byte stream,
        // aligned to NAL boundaries. This is a deviation from RFC 6184.

        // I don't expect the marker bit to be trustworthy in this case.
        // It feels like the whole data blob is split into access units, though,
        // at least in some practical cases.
        h264_parser.parseByteStreamNals (nal_pages->first,
                                         nal_offs,
                                         nal_len,
                                         timestamp /* pts */,
                                         timestamp /* dts */);
    }
}

H264Parser::Callbacks const RtspClient::h264_callbacks = {
    h264_parameterSets,
    h264_accessUnit
};

mt_mutex (mutex) void
RtspClient::h264_parameterSets (PagePool::PageListHead * const page_list,
                                Size                     const msg_offs,
                                Size                     const msg_len,
                                Time                     const pts,
                                Time                     const dts,
                                void                   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    logS (frames, _self_func);

    {
        VideoMessage video_msg;
        video_msg.codec_id    = VideoCodecId::AVC;
        video_msg.frame_type  = VideoFrameType::AvcSequenceHeader;
        video_msg.pts_nanosec = (Uint64) pts * 10 / 9 * 10000 + 10000000000;
        video_msg.dts_nanosec = (Uint64) dts * 10 / 9 * 10000 + 10000000000;
        video_msg.page_pool   = self->page_pool;
        video_msg.page_list   = *page_list;
        video_msg.msg_len     = msg_len;
        video_msg.msg_offset  = msg_offs;

        self->stream->fireVideoMessage (&video_msg);
    }
}

mt_mutex (mutex) void
RtspClient::h264_accessUnit (PagePool::PageListHead * const page_list,
                             Size                     const msg_offs,
                             Size                     const msg_len,
                             bool                     const is_keyframe,
                             Time                     const pts,
                             Time                     const dts,
                             void                   * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    logS (frames, _self_func);

    self->last_frame_time_millisec = getTimeMilliseconds();
    self->got_first_frame = true;

    {
        VideoMessage video_msg;
        video_msg.codec_id    = VideoCodecId::AVC;
        video_msg.frame_type  = (is_keyframe ? VideoFrameType::KeyFrame : VideoFrameType::InterFrame);
        video_msg.pts_nanosec = (Uint64) pts * 10 / 9 * 10000 + 10000000000;
        video_msg.dts_nanosec = (Uint64) dts * 10 / 9 * 10000 + 10000000000;
        video_msg.page_pool   = self->page_pool;
        video_msg.page_list   = *page_list;
        video_msg.msg_len     = msg_len;
        video_msg.msg_offset  = msg_offs;

        self->stream->fireVideoMessage (&video_msg);
    }
}

mt_mutex (mutex) void
RtspClient::nextStateAction ()
{
    while (doNextStateAction ());
}

mt_mutex (mutex) bool
RtspClient::doNextStateAction ()
{
    switch (state.val()) {
        case State::Initial: {
            return stateAction_sendOptions ();
        } break;
        case State::OptionsSent: {
            return stateAction_sendDescribe ();
        } break;
        case State::DescribeSent: {
            return stateAction_sendSetupAudio ();
        } break;
        case State::SetupAudioSent: {
            return stateAction_sendSetupVideo ();
        } break;
        case State::SetupVideoSent: {
            return stateAction_sendPlay ();
        } break;
        case State::PlaySent: {
          /* No-op */
        } break;
        case State::InterleavedData: {
          /* No-op */
        } break;
    }

    return false;
}

mt_mutex (mutex) void
RtspClient::repeatLastStateAction ()
{
    switch (state.val()) {
        case State::Initial:
            logD_ (_this_func, "unexpected repeatLastStateAction() call");
            break;
        case State::OptionsSent:
            stateAction_sendOptions ();
            break;
        case State::DescribeSent:
            stateAction_sendDescribe ();
            break;
        case State::SetupAudioSent:
            stateAction_sendSetupAudio ();
            break;
        case State::SetupVideoSent:
            stateAction_sendSetupVideo ();
            break;
        case State::PlaySent:
            stateAction_sendPlay ();
            break;
        case State::InterleavedData:
            sendGetParameter ();
            break;
    }
}

mt_mutex (mutex) bool
RtspClient::stateAction_sendOptions ()
{
    logD_ (_this_func_);

    state = State::OptionsSent;
    last_method = "OPTIONS";
    last_uri = newString (target_uri);

    MOMENT_SERVER__HEADERS_DATE
    conn_sender->send (page_pool,
                       true /* do_flush */,
                       "OPTIONS ", target_uri, " RTSP/1.0\r\n"
                       "CSeq: ", rtsp_cseq, "\r\n",
                       getAuthHeader (),
                       "Date: ", ConstMemory (date_buf, date_len), "\r\n" \
                       "\r\n");
    ++rtsp_cseq;

    return false;
}

mt_mutex (mutex) bool
RtspClient::stateAction_sendDescribe ()
{
    logD_ (_this_func, "sending DESCRIBE");

    state = State::DescribeSent;
    last_method = "DESCRIBE";
    last_uri = newString (target_uri);

    MOMENT_SERVER__HEADERS_DATE
    conn_sender->send (
            page_pool,
            true /* do_flush */,
            "DESCRIBE ", target_uri, " RTSP/1.0\r\n"
            "CSeq: ", rtsp_cseq, "\r\n",
            "Accept: application/sdp\r\n",
            getAuthHeader (),
            "Date: ", ConstMemory (date_buf, date_len), "\r\n" \
            "\r\n");
    ++rtsp_cseq;

    return false;
}

mt_mutex (mutex) bool
RtspClient::stateAction_doSendSetup (RtpConnection * const mt_nonnull rtp_conn)
{
    logD_ (_this_func_);

    if (!rtp_conn->control_url || !rtp_conn->enable)
        return true;

    last_method = "SETUP";
    last_uri = newString (rtp_conn->control_url);

    /* Unnecessary?
    // TODO Live555 ignores ssrc= ? What's that for?
    ssrc = (Uint32) getUnixtime();
    logD_ (_this_func, "ssrc: ", fmt_hex, ssrc);
    */

    MOMENT_SERVER__HEADERS_DATE
    conn_sender->send (
            page_pool,
            true /* do_flush */,
            "SETUP ", rtp_conn->control_url, " RTSP/1.0\r\n"
            "CSeq: ", rtsp_cseq, "\r\n",
            getAuthHeader (),
            (tcp_interleaved ?
                      makeString ("Transport: RTP/AVP/TCP;unicast;interleaved=",
                                  (rtp_conn->is_audio ? ConstMemory ("0-1") : ConstMemory ("2-3")),
                                  "\r\n")->mem()
                    : makeString ("Transport: RTP/AVP;unicast;client_port=",
                                  rtp_conn->rtp_port, "-", rtp_conn->rtcp_port,
                                  /* Unnecessary? ";ssrc=", ssrc, */ "\r\n")->mem()),
            (session_id ? makeString ("Session: ", session_id, "\r\n")->mem() : ConstMemory()),
            "Date: ", ConstMemory (date_buf, date_len), "\r\n" \
            "\r\n");
    ++rtsp_cseq;

    return false;
}

mt_mutex (mutex) bool
RtspClient::stateAction_sendSetupAudio ()
{
    logD_ (_this_func_);

    state = State::SetupAudioSent;
    return stateAction_doSendSetup (&rtp_audio);
}

mt_mutex (mutex) bool
RtspClient::stateAction_sendSetupVideo ()
{
    logD_ (_this_func_);

    state = State::SetupVideoSent;
    return stateAction_doSendSetup (&rtp_video);
}

mt_mutex (mutex) bool
RtspClient::stateAction_sendPlay ()
{
    logD_ (_this_func_);

    last_method = "PLAY";
    last_uri = newString (target_uri);

    MOMENT_SERVER__HEADERS_DATE
    conn_sender->send (
            page_pool,
            true /* do_flush */,
            "PLAY ", target_uri, " RTSP/1.0\r\n"
            "CSeq: ", rtsp_cseq, "\r\n",
            getAuthHeader (),
            "Range: npt=0-\r\n",
            (session_id ? makeString ("Session: ", session_id, "\r\n")->mem() : ConstMemory()),
            "Date: ", ConstMemory (date_buf, date_len), "\r\n" \
            "\r\n");
    ++rtsp_cseq;
    state = State::PlaySent;

    return false;
}

namespace {
    struct RtspStatus {
        ConstMemory rtsp_version;
        Uint32      status_code;
        ConstMemory reason_phrase;
    };
}

static Size skipSpHt (ConstMemory const mem,
                      Size              offs)
{
    for (; offs < mem.len(); ++offs) {
        if (   mem.buf() [offs] != 32 /* SP */
            && mem.buf() [offs] !=  9 /* HT */)
        {
            break;
        }
    }
    return offs;
}

static Size findSpHt (ConstMemory const mem,
                      Size              offs)
{
    for (; offs < mem.len(); ++offs) {
        if (   mem.buf() [offs] == 32 /* SP */
            || mem.buf() [offs] ==  9 /* HT */)
        {
            break;
        }
    }
    return offs;
}

static bool parseRtspStatusLine (ConstMemory   const line,
                                 RtspStatus  * const mt_nonnull ret_status)
{
    Size offs = 0;

    offs = skipSpHt (line, offs);
    Size const rtsp_version_beg = offs;
    offs = findSpHt (line, offs);
    Size const rtsp_version_len = offs - rtsp_version_beg;

    offs = skipSpHt (line, offs);
    Size const status_code_beg = offs;
    offs = findSpHt (line, offs);
    Size const status_code_len = offs - status_code_beg;

    offs = skipSpHt (line, offs);
    Size const reason_phrase_beg = offs;
    Size const reason_phrase_len = line.len() - offs;

    if (   rtsp_version_len == 0
        || status_code_len == 0)
    {
        logD_ (_func, "bad request line: ", line);
        return false;
    }

    ConstMemory const rtsp_version    (line.buf() + rtsp_version_beg,  rtsp_version_len);
    ConstMemory const status_code_mem (line.buf() + status_code_beg,   status_code_len);
    ConstMemory const reason_phrase   (line.buf() + reason_phrase_beg, reason_phrase_len);

    {
        ConstMemory const rtsp_mem = "RTSP";
        if (rtsp_version.len() < rtsp_mem.len()
            || !equal (ConstMemory (rtsp_version.buf(), rtsp_mem.len()), rtsp_mem))
        {
            return false;
        }
    }

    Uint32 status_code = 0;
    if (!strToUint32_safe (status_code_mem, &status_code, 10)) {
      // If there's no numerical status code, then this is probably
      // an RTSP request, not a response.
        return false;
    }
    
    ret_status->rtsp_version  = rtsp_version;
    ret_status->status_code   = status_code;
    ret_status->reason_phrase = reason_phrase;

    return true;
}

static ConstMemory stripRtspSessionId (ConstMemory const session_id)
{
    if (session_id.len() == 0)
        return session_id;

    Byte const * const semicolon_ptr = (Byte const *) memchr (session_id.buf(), ';', session_id.len());
    if (semicolon_ptr)
        return session_id.region (0, semicolon_ptr - session_id.buf());

    return session_id;
}

static ConstMemory stripHostFromUri (ConstMemory const uri)
{
    Size start = 0;
    unsigned i = 0;
    while (start < uri.len()) {
        if (i >= 3)
            break;

        if (uri.mem() [start] == '/')
            ++i;

        ++start;
    }

    return uri.region (start);
}

void
RtspClient::sendUdpNatPinholePackets (HttpRequest * const mt_nonnull req)
{
    if (tcp_interleaved)
        return;

    RtspTransportHeaderInfo transport_info;
    parseRtspTransportHeader (req, &transport_info);

    RtpConnection *rtp_conn;
    if (state == State::SetupAudioSent)
        rtp_conn = &rtp_audio;
    else
        rtp_conn = &rtp_video;

    if (transport_info.got_server_port) {
        Byte buf [128];
        memset (buf, 0, sizeof (buf));

        if (transport_info.server_port_rtp != 0) {
            IpAddress addr;
            setIpAddress (server_addr.ip_addr, transport_info.server_port_rtp, &addr);
            rtp_conn->rtp_sender->setDatagramSendtoAddress (addr);
            for (unsigned i = 0; i < 5; ++i)
                rtp_conn->rtp_sender->send (page_pool, true /* do_flush */, ConstMemory::forObject (buf));
        }

        if (transport_info.server_port_rtcp != 0) {
            IpAddress addr;
            setIpAddress (server_addr.ip_addr, transport_info.server_port_rtcp, &addr);
            rtp_conn->rtcp_sender->setDatagramSendtoAddress (addr);
            for (unsigned i = 0; i < 5; ++i)
                rtp_conn->rtcp_sender->send (page_pool, true /* do_flush */, ConstMemory::forObject (buf));
        }
    }
}

static bool isWhitespace (Byte const c)
{
    return    c == '\r'
           || c == '\n'
           || c == ' '
           || c == '\t'
           || c == 9 /* HT */;
}

static bool isAlphanumeric (Byte const c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            (c == '-') ||
            (c == '_'));
}

static bool isOneOf (Byte        const c,
                     ConstMemory const oneof)
{
    Byte const *buf = oneof.buf();
    Size        len = oneof.len();

    while (len) {
        if (c == *buf)
            return true;

        ++buf;
        --len;
    }

    return false;
}

static void skipWhitespace (Byte const ** const mt_nonnull ret_buf,
                                 Size        * const mt_nonnull ret_len)
{
    Byte const *buf = *ret_buf;
    Size        len = *ret_len;

    while (len) {
        if (!isWhitespace (*buf))
            break;

        ++buf;
        --len;
    }

    *ret_buf = buf;
    *ret_len = len;
}

static void skipWhitespace (ConstMemory * const mt_nonnull mem)
{
    Byte const *buf = mem->buf();
    Size        len = mem->len();

    skipWhitespace (&buf, &len);

    *mem = ConstMemory (buf, len);
}

static void skipWhitespaceOrOneOf (ConstMemory * const mt_nonnull mem,
                                   ConstMemory   const oneof)
{
    Byte const *buf = mem->buf();
    Size        len = mem->len();

    while (len) {
        if (!(   isWhitespace (*buf)
              || isOneOf (*buf, oneof)))
        {
            break;
        }

        ++buf;
        --len;
    }

    *mem = ConstMemory (buf, len);
}

static void skipToAfterWhitespace (ConstMemory * const mem)
{
    Byte const *buf = mem->buf();
    Size        len = mem->len();

    while (len) {
        if (isWhitespace (*buf)) {
            *mem = ConstMemory (buf, len);
            skipWhitespace (mem);
            return;
        }

        ++buf;
        --len;
    }

    *mem = ConstMemory();
}

static ConstMemory skipToAfterWhitespaceOrOneOf (ConstMemory * const mt_nonnull mem,
                                                 ConstMemory   const oneof)
{
    Byte const *buf = mem->buf();
    Size        len = mem->len();

    while (len) {
        if (   isWhitespace (*buf)
            || isOneOf (*buf, oneof))
        {
            ConstMemory const result (mem->buf(), buf - mem->buf());

            *mem = ConstMemory (buf + 1, len - 1);
            skipWhitespaceOrOneOf (mem, oneof);

            return result;
        }

        ++buf;
        --len;
    }

    ConstMemory const result = *mem;
    *mem = ConstMemory();
    return result;
}

static void skipToNonAlphanumeric (Byte const ** const mt_nonnull ret_buf,
                                   Size        * const mt_nonnull ret_len)
{
    Byte const *buf = *ret_buf;
    Size        len = *ret_len;

    while (len) {
        if (!isAlphanumeric (*buf))
            break;

        ++buf;
        --len;
    }

    *ret_buf = buf;
    *ret_len = len;
}

#if 0
// Unused
static ConstMemory skipToNonAlphanumeric (ConstMemory * const mt_nonnull mem)
{
    Byte const *buf = mem->buf();
    Size        len = mem->len();

    skipToNonAlphanumeric (&buf, &len);
    ConstMemory const token (mem->buf(), buf - mem->buf());

    *mem = ConstMemory (buf, len);
    return token;
}
#endif

static ConstMemory skipToken (Byte const ** const mt_nonnull ret_buf,
                              Size        * const mt_nonnull ret_len)
{
    skipWhitespace (ret_buf, ret_len);
    Byte const * const name_beg = *ret_buf;
    skipToNonAlphanumeric (ret_buf, ret_len);
    Byte const * const name_end = *ret_buf;

    return ConstMemory (name_beg, name_end - name_beg);
}

static ConstMemory skipToken (ConstMemory * const mt_nonnull mem)
{
    Byte const *buf = mem->buf();
    Size        len = mem->len();

    ConstMemory const token = skipToken (&buf, &len);

    *mem = ConstMemory (buf, len);
    return token;
}

static Byte const * skipToAfterQuote (Byte const ** const mt_nonnull ret_buf,
                                      Size        * const mt_nonnull ret_len)
{
    if (!*ret_len)
        return NULL;

    Byte const * const ptr = (Byte const *) memchr (*ret_buf, '"', *ret_len);
    if (!ptr)
        return NULL;

    *ret_len -= ptr + 1 - *ret_buf;
    *ret_buf  = ptr + 1;
    return ptr;
}

static ConstMemory skipToAfterSymbol (ConstMemory * const mt_nonnull mem,
                                      Byte          const c)
{
    if (!mem->len())
        return ConstMemory();

    Byte const * const ptr = (Byte const *) memchr (mem->buf(), c, mem->len());
    if (!ptr) {
        ConstMemory const result = *mem;
        *mem = ConstMemory();
        return result;
    }

    ConstMemory const result (mem->buf(), ptr - mem->buf());
    *mem = ConstMemory (ptr + 1, mem->len() - (ptr + 1 - mem->buf()));
    return result;
}

static ConstMemory skipQuotedString (Byte const ** const mt_nonnull ret_buf,
                                     Size        * const mt_nonnull ret_len,
                                     Byte const  * const orig_buf,
                                     Byte const  * const lower_buf)
{
    Byte const * const quote_beg = skipToAfterQuote (ret_buf, ret_len);
    if (!quote_beg)
        return ConstMemory();

    Byte const * const quote_end = skipToAfterQuote (ret_buf, ret_len);
    if (!quote_beg)
        return ConstMemory();

    return ConstMemory (orig_buf + (quote_beg + 1 - lower_buf),
                        quote_end - quote_beg - 1);
}

mt_sync_domain (http_server_frontend) StRef<String>
RtspClient::getAuthHeader ()
{
    if (auth_digest)
        return makeDigestAuthHeader ();

  // Basic or no auth.

    return auth_header;
}

mt_sync_domain (http_server_frontend) StRef<String>
RtspClient::makeDigestAuthHeader ()
{
    Byte kd [32];
    {
        Byte h_a1 [32];
        {
            StRef<String> const a1 = makeString (login, ":", auth_realm, ":", password);
            getMd5HexAscii (a1, Memory::forObject (h_a1));
        }
        Byte h_a2 [32];
        {
            StRef<String> const a2 = makeString (last_method, ":", last_uri);
            getMd5HexAscii (a2, Memory::forObject (h_a2));
        }

        StRef<String> const kd_arg = makeString (ConstMemory::forObject (h_a1),
                                                 ":", auth_nonce, ":",
                                                 ConstMemory::forObject (h_a2));
        getMd5HexAscii (kd_arg, Memory::forObject (kd));
    }

    return makeString ("Authorization: Digest "
                       "username=\"", login, "\","
                       "realm=\"",    auth_realm, "\","
                       "nonce=\"",    auth_nonce, "\","
                       "uri=\"",      last_uri, "\","
                       "response=\"", ConstMemory::forObject (kd), "\"",
                       auth_opaque,
                       "\r\n");
//    logD_ (_this_func, "auth_header: ", ConstMemory (auth_header->buf(), auth_header->len() - 2 /* \r\n */));
}

mt_sync_domain (http_server_frontend) void
RtspClient::rtspRequest_401_Unauthorized (HttpRequest * const mt_nonnull req)
{
    bool do_close = true;

    ConstMemory const www_authenticate = req->getHeader ("www-authenticate");
    if (!www_authenticate.len()) {
        logE_ (_this_func, "no WWW-Authenticate response header");
        do_close = true;
    } else {
        logD_ (_this_func, "WWW-Authenticate: ", www_authenticate);
    }

    mutex.lock ();

    if (auth_required) {
        logE_ (_this_func, "authentication failed: 401 unauthorized");
    } else
    if (state != State::DescribeSent) {
        logE_ (_this_func, "unexpected 401 unauthorized");
    } else
    if (!String::len (login)) {
        logE_ (_this_func, "login required: 401 unauthorized");
    } else {
        do_close = false;
    }

    if (do_close) {
        mt_unlocks (mutex) destroySelf (DisconnectReason::AccessDenied);
        return;
    }

    auth_required = true;

    StRef<String> const lower_www_authenticate = newString (www_authenticate);
    toLowercase (lower_www_authenticate->mem());
    logD_ (_this_func, "lower_www_authenticate: \"", lower_www_authenticate, "\"");

    Byte const *buf = lower_www_authenticate->buf();
    Size        len = lower_www_authenticate->len();

    ConstMemory const auth_scheme = skipToken (&buf, &len);

    if (equal (auth_scheme, "basic")) {
        logD_ (_this_func, "Basic authentication");
        StRef<String> const raw_loginpass = makeString (login, ":", password);

        Byte * const loginpass_buf = new (std::nothrow) Byte [base64EncodedBufferSize (raw_loginpass->len())];
        assert (loginpass_buf);
        Size const loginpass_len = base64Encode (raw_loginpass, loginpass_buf);
        ConstMemory const loginpass_mem (loginpass_buf, loginpass_len);

        auth_header = makeString ("Authorization: Basic ", loginpass_mem, "\r\n");

        delete[] loginpass_buf;

        auth_digest = false;
    } else {
        if (!equal (auth_scheme, "digest")) {
            logD_ (_this_func, "unknown authentication scheme, assuming Digest");
        } else {
            logD_ (_this_func, "Digest authentication");
        }

        ConstMemory realm;
        ConstMemory nonce;

        bool got_opaque = false;
        ConstMemory opaque;

        while (len) {
            ConstMemory const param_name = skipToken (&buf, &len);
            if (!param_name.len())
                break;

            logD_ (_this_func, "param_name \"", param_name, "\"");

            if (equal (param_name, "realm")) {
                realm = skipQuotedString (&buf, &len, www_authenticate.buf(), lower_www_authenticate->buf());
                logD_ (_this_func, "realm: \"", realm, "\"");
            } else
            if (equal (param_name, "nonce")) {
                nonce = skipQuotedString (&buf, &len, www_authenticate.buf(), lower_www_authenticate->buf());
                logD_ (_this_func, "nonce: \"", nonce, "\"");
            } else
            if (equal (param_name, "opaque")) {
                got_opaque = true;
                opaque = skipQuotedString (&buf, &len, www_authenticate.buf(), lower_www_authenticate->buf());
                logD_ (_this_func, "opaque: \"", opaque, "\"");
            }

            Byte const * const comma = (Byte const *) memchr (buf, ',', len);
            if (!comma) {
                break;
            }
            len -= comma + 1 - buf;
            buf  = comma + 1;
        }

        auth_realm = newString (realm);
        auth_nonce = newString (nonce);
        if (got_opaque)
            auth_opaque = makeString (",opaque=\"", opaque, "\"");
        else
            auth_opaque = NULL;

        auth_digest = true;
        auth_header = NULL;
    }

    repeatLastStateAction ();
    mutex.unlock ();
}

mt_unlocks (mutex) void
RtspClient::rtspRequest_OptionsReply ()
{
    nextStateAction ();
    mutex.unlock ();
}

mt_unlocks (mutex) void
RtspClient::rtspRequest_DescribeReply (HttpRequest * const mt_nonnull req,
                                       Uint32        const status_code)
{
    preassembly_buffer.reset (req->getContentLength());

    if (status_code != 200) {
        logE_ (_this_func, "DESCRIBE error");
        mt_unlocks (mutex) destroySelf (DisconnectReason::ResourceNotFound);
        return;
    }

    if (!req->hasBody()) {
        logE_ (_this_func, "empty DESCRIBE respose");
        mt_unlocks (mutex) destroySelf (DisconnectReason::ProtocolError);
        return;
    }

    got_describe_reply = true;
    mutex.unlock ();

  // State transition will be made in preassemblyData().
}

mt_unlocks (mutex) void
RtspClient::rtspRequest_SetupReply (HttpRequest * const mt_nonnull req)
{
    ConstMemory const sid = stripRtspSessionId (req->getHeader ("session"));
    logD_ (_this_func, "Session: ", sid);
    if (!session_id) {
        if (sid.buf())
            session_id = newString (sid);
    } else {
        if (!equal (session_id->mem(), sid)) {
            logW_ (_this_func, "different session ids for audio "
                   "(", session_id, ") and video (", sid, ")");
        }
    }

    sendUdpNatPinholePackets (req);

    nextStateAction ();
    mutex.unlock ();
}

mt_unlocks (mutex) void
RtspClient::rtspRequest_PlayReply (HttpRequest * const mt_nonnull req)
{
    ConstMemory rtp_info = req->getHeader ("rtp-info");
    logD_ (_this_func, "RTP-Info: ", rtp_info);

    skipWhitespace (&rtp_info);
    while (rtp_info.len()) {
        ConstMemory line = skipToAfterWhitespaceOrOneOf (&rtp_info, ConstMemory (",", 1));
        logD_ (_this_func, "line: \"", line, "\"");

        ConstMemory url;

        bool   got_seq = false;
        Uint64 seq = 0;

        bool   got_rtptime = false;
        Uint64 rtptime = 0;

        while (line.len()) {
            ConstMemory const param_name = skipToken (&line);
            skipToAfterWhitespaceOrOneOf (&line, ConstMemory ("=", 1));
            ConstMemory const param_value = skipToAfterWhitespaceOrOneOf (&line, ConstMemory (";", 1));

            if (equal (param_name, "url")) {
                url = param_value;
                logD_ (_this_func, "url: ", url);
            } else
            if (equal (param_name, "seq")) {
                logD_ (_this_func, "seq: ", param_value);

                if (strToUint64 (param_value, &seq, NULL, 10))
                    got_seq = true;
                else
                    logW_ (_this_func, "bad seq value: ", param_value);
            } else
            if (equal (param_name, "rtptime")) {
                logD_ (_this_func, "rtptime: ", param_value);

                if (strToUint64 (param_value, &rtptime, NULL, 10))
                    got_rtptime = true;
                else
                    logW_ (_this_func, "bad rtptime vlaue: ", param_value);
            }
        }

        // Hostname component of the url is unstable, i.e. localhost<->127.0.0.1.
        ConstMemory const url_nohost = stripHostFromUri (url);
        if (rtp_audio.control_url && equal (stripHostFromUri (rtp_audio.control_url->mem()), url_nohost)) {
            logD_ (_this_func, "setting audio params");
            rtp_audio.got_expected_seq     = got_seq;
            rtp_audio.expected_seq         = seq;
            rtp_audio.got_expected_rtptime = got_rtptime;
            rtp_audio.expected_rtptime     = rtptime;
            rtp_audio.initial_rtptime      = rtptime;
        } else
        if (rtp_video.control_url && equal (stripHostFromUri (rtp_video.control_url->mem()), url_nohost)) {
            logD_ (_this_func, "setting video params");
            rtp_video.got_expected_seq     = got_seq;
            rtp_video.expected_seq         = seq;
            rtp_video.got_expected_rtptime = got_rtptime;
            rtp_video.expected_rtptime     = rtptime;
            rtp_video.initial_rtptime      = rtptime;
        }
    }

    state = State::InterleavedData;

    if (!ping_timer) {
        ping_timer = thread_ctx->getTimers()->addTimer (
                             CbDesc<Timers::TimerCallback> (pingTimerTick, this, this),
                             10    /* time_seconds */,
                             true  /* periodical */,
                             false /* auto_delete */);
    }

    mutex.unlock ();
}

void
RtspClient::rtspRequest_Request (HttpRequest * const mt_nonnull req)
{
    ConstMemory const cseq_mem = req->getHeader ("cseq");

    logD_ (_this_func, req->getRequestLine());
    conn_sender->send (page_pool,
                       true /* do_flush */,
                       "RTSP/1.0 501 Not Implemented\r\n"
                       "CSeq: ", cseq_mem, "\r\n"
                       "\r\n");
}

void
RtspClient::rtspRequest (HttpRequest * const mt_nonnull req,
                         bool        * const mt_nonnull /* ret_block_input */,
                         void        * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    logD_ (_self_func, "req: ", req->getRequestLine());

    RtspStatus status;
    {
        bool const got_status = parseRtspStatusLine (req->getRequestLine(), &status);
        if (!got_status) {
            self->rtspRequest_Request (req);
            return;
        }

        logD_ (_func, "RTSP status: "
               "rtsp_version \"",  status.rtsp_version,  "\", "
               "status_code ",     status.status_code,   ", "
               "reason_phrase \"", status.reason_phrase, "\"");

        if (status.status_code == 401) {
            self->rtspRequest_401_Unauthorized (req);
            return;
        } else {
            self->auth_required = false;
        }
    }

    self->mutex.lock ();

    {
        Uint64 cseq = 0;
        if (!strToUint64 (req->getHeader ("cseq"), &cseq, NULL, 10)) {
            logD_ (_self_func, "no CSeq");
        } else {
            if (cseq != self->rtsp_cseq - 1) {
                self->mutex.unlock ();
                logD_ (_func, "unexpected CSeq: ", cseq, " (expected ", self->rtsp_cseq - 1, ")");
                return;
            }
        }
    }

    {
        ConstMemory const content_base_mem = req->getHeader ("content-base");
        if (content_base_mem.len()) {
            logD_ (_self_func, "content-base: \"", content_base_mem, "\"");
            self->target_uri = newString (stripTrailingSlashes (content_base_mem));
        }
    }

    switch (self->state.val()) {
        case State::Initial: {
            self->mutex.unlock ();
            logW_ (_self_func, "ignoring unexpected early RTSP response: ", req->getRequestLine());
        } break;
        case State::OptionsSent: {
            mt_unlocks (mutex) self->rtspRequest_OptionsReply ();
        } break;
        case State::DescribeSent: {
            mt_unlocks (mutex) self->rtspRequest_DescribeReply (req, status.status_code);
        } break;
        case State::SetupAudioSent:
        case State::SetupVideoSent: {
            mt_unlocks (mutex) self->rtspRequest_SetupReply (req);
        } break;
        case State::PlaySent: {
            mt_unlocks (mutex) self->rtspRequest_PlayReply (req);
        } break;
        case State::InterleavedData: {
            // No-op
            self->mutex.unlock ();
        } break;
    }
}

void
RtspClient::rtspMessageBody (HttpRequest * const mt_nonnull /* req */,
                             Memory        const mem,
                             bool          const end_of_request,
                             Size        * const mt_nonnull /* ret_accepted */,
                             bool        * const mt_nonnull /* ret_block_input */,
                             void        * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    self->mutex.lock ();
    if (   self->state == State::DescribeSent
        && self->got_describe_reply)
    {
        logD_ (_self_func, "preassembling");
        self->preassembly_buffer.processData (mem, end_of_request);

        if (self->close_pending) {
            mt_unlocks (mutex) self->destroySelf (self->close_pending__disconnect_reason);
            return;
        }
    }

    self->mutex.unlock ();

  // *ret_accepted is pre-set to available body part length
}

void
RtspClient::rtspClosed (HttpRequest      * const /* req */,
                        Exception        * const exc_,
//                        DisconnectReason   const disconnect_reason,
                        void             * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    if (exc_)
        logD_ (_self_func, "exception: ", exc_->toString());
    else
        logD_ (_self_func_);

    self->mutex.lock ();
    mt_unlocks (mutex) self->destroySelf (/* disconnect_reason */ DisconnectReason::TodoCategorizeMe);
}

PreassemblyBuffer::Frontend const RtspClient::preassembly_frontend = {
    preassemblyData,
    NULL /* dataTrail */
};

namespace {
    struct SdpInfo
    {
        bool got_audio_control_uri  = false;
        bool got_audio_payload_type = false;
        bool got_audio_is_aac       = false;
        bool got_audio_aac_config   = false;
        Uint32 audio_payload_type   = 0;
        Uint32 audio_clock_rate     = 0;
        Uint32 audio_channels       = 0;
        ConstMemory aac_config;

        bool got_video_control_uri      = false;
        bool got_video_payload_type = false;
        bool got_video_is_avc       = false;
        Uint32 video_payload_type   = 0;
        ConstMemory avc_parameter_sets;
        ConstMemory avc_profile_level_id;
    };
}

static void parseSdpRtpmap_Audio (ConstMemory  line,
                                  SdpInfo     * const mt_nonnull sdp_info)
{
    skipToAfterWhitespace (&line);
    ConstMemory const encoding_name  = skipToAfterSymbol (&line, '/');
    ConstMemory const clock_rate_mem = skipToAfterSymbol (&line, '/');
    ConstMemory const encoding_parameters_mem = line;

    logD_ (_func, "encoding_name ", encoding_name, ", clock_rate ", clock_rate_mem);

    {
        ConstMemory const mpeg4_generic = "mpeg4-generic";
        if (encoding_name.len() == mpeg4_generic.len()) {
            Byte encoding_name_lower [mpeg4_generic.len()];
            memcpy (encoding_name_lower, encoding_name.buf(), sizeof (encoding_name_lower));
            toLowercase (Memory (encoding_name_lower, sizeof (encoding_name_lower)));

            if (equal (ConstMemory (encoding_name_lower, mpeg4_generic.len()), mpeg4_generic))
                sdp_info->got_audio_is_aac = true;
        }

        if (!sdp_info->got_audio_is_aac) {
            logW_ (_func, "rtpmap encoding_name is not MPEG4-GENERIC: \"", encoding_name, "\"");
            return;
        }
    }

    Uint32 clock_rate = 44100;
    if (!strToUint32 (clock_rate_mem, &clock_rate, NULL, 10))
        logW_ (_func, "bad rtpmap clock_rate value: ", clock_rate_mem);

    Uint32 channels = 1;
    if (encoding_parameters_mem.len()) {
        if (!strToUint32 (encoding_parameters_mem, &channels, NULL, 10))
            logW_ (_func, "bad rtpmap encoding_parameters value: ", encoding_parameters_mem);
    }

    logD_ (_func, "parsed clock_rate: ", clock_rate, ", channels: ", channels);

    sdp_info->audio_clock_rate = clock_rate;
    sdp_info->audio_channels = channels;
}

static void parseSdpRtpmap (ConstMemory   const line,
                            bool          const parsing_audio,
                            bool          const parsing_video,
                            SdpInfo     * const mt_nonnull sdp_info)
{
    ConstMemory const a_rtpmap = "a=rtpmap:";
    assert (line.len() >= a_rtpmap.len());
    ConstMemory const payload_type_value = line.region (a_rtpmap.len());

    Uint32 payload_type = 0;
    if (!strToUint32 (payload_type_value, &payload_type, NULL /* endptr */, 10)) {
        logW_ (_func, "bad rtpmap payload type: ", payload_type_value);
        return;
    }

    if (parsing_audio) {
        sdp_info->audio_payload_type = payload_type;
        sdp_info->got_audio_payload_type = true;

        parseSdpRtpmap_Audio (line, sdp_info);
    } else
    if (parsing_video) {
        sdp_info->video_payload_type = payload_type;
        sdp_info->got_video_payload_type = true;

#warning TODO verify AVC
        sdp_info->got_video_is_avc = true;
    }
}

static void parseSdpFmtp (ConstMemory  line,
                          bool          const parsing_audio,
                          bool          const parsing_video,
                          SdpInfo     * const mt_nonnull sdp_info)
{
    /*
    logD_ (_func,
           "parsing_audio ", parsing_audio, ", "
           "parsing_video ", parsing_video, ", "
           "line: \"", line, "\"");

    logHex_ (LogLevel::Debug, line, _func);
    */

    skipToAfterWhitespace (&line);
    while (line.len()) {
        ConstMemory const param_name = skipToken (&line);
        skipToAfterWhitespaceOrOneOf (&line, ConstMemory ("=", 1));
        ConstMemory const param_value = skipToAfterWhitespaceOrOneOf (&line, ConstMemory (";", 1));

        /*
        logD_ (_func, "param_name: \"", param_name, "\"");
        */

        if (parsing_audio) {
            if (equal (param_name, "config")) {
                sdp_info->aac_config = param_value;
                sdp_info->got_audio_aac_config = true;
                logD_ (_func, "aac_config: ", sdp_info->aac_config);
            }
        } else
        if (parsing_video) {
            if (equal (param_name, "sprop-parameter-sets")) {
                sdp_info->avc_parameter_sets = param_value;
                logD_ (_func, "parameter sets: ", sdp_info->avc_parameter_sets);
            } else
            if (equal (param_name, "profile-level-id")) {
                sdp_info->avc_profile_level_id = param_value;
                logD_ (_func, "profile level id: ", sdp_info->avc_profile_level_id);
            }
        }
    }
}

mt_mutex (mutex) void
RtspClient::preassemblyData (ConstMemory   const mem,
                             bool          const limit_overflow,
                             void        * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    if (self->closed || self->close_pending)
        return;

    if (!(   self->state == State::DescribeSent
          && self->got_describe_reply))
    {
        logW_ (_self_func, "unexpected data");
        return;
    }

    if (limit_overflow) {
        logE_ (_self_func, "preassembly limit exceeded");
        self->close_pending = true;
        self->close_pending__disconnect_reason = DisconnectReason::ProtocolError;
        return;
    }

  // SDP parsing

    if (logLevelOn_ (LogLevel::Debug)) {
        logLock ();
        logD_locked_ (_self_func, "SDP len ", mem.len());
        hexdump (logs, mem);
        logs->print (mem);
        logUnlock ();
    }

    bool audio_parsed = false;
    bool video_parsed = false;
    bool parsing_audio = false;
    bool parsing_video = false;

    SdpInfo sdp_info;

    Byte const *buf = mem.mem();
    Size line_beg = 0;
    Size pos = 0;
    while (pos < mem.len()) {
        if (buf [pos] != '\r' && buf [pos] != '\n') {
            ++pos;
            continue;
        }

        ConstMemory line = ConstMemory (buf + line_beg, pos - line_beg);
        logD_ (_self_func, "line: ", line);

        if (line.mem() [0] == 'm') {
            parsing_audio = false;
            parsing_video = false;

            ConstMemory const m_audio = "m=audio";
            if (!audio_parsed && line.len() >= m_audio.len() && equal (line.region (0, m_audio.len()), m_audio)) {
                audio_parsed = true;
                parsing_audio = true;
                logD_ (_self_func, "parsing audio");
            }

            ConstMemory const m_video = "m=video";
            if (!video_parsed && line.len() >= m_video.len() && equal (line.region (0, m_video.len()), m_video)) {
                video_parsed = true;
                parsing_video = true;
                logD_ (_self_func, "parsing video");
            }
        } else
        if (line.mem()[0] == 'a') {
            ConstMemory const a_control = "a=control:";
            ConstMemory const a_rtpmap  = "a=rtpmap:";
            ConstMemory const a_fmtp    = "a=fmtp:";

            if (line.len() >= a_control.len() && equal (line.region (0, a_control.len()), a_control)) {
                ConstMemory const control_value = line.region (a_control.len());

                StRef<String> ctl_uri;
                {
                    ConstMemory const rtsp_prefix = "rtsp://";
                    if (control_value.len() >= rtsp_prefix.len()
                        && equal (control_value.region (0, rtsp_prefix.len()), rtsp_prefix))
                    {
                        ctl_uri = makeString (control_value);
                    } else {
                        ctl_uri = makeString (self->target_uri, "/", control_value);
                    }
                }

                if (parsing_audio) {
                    self->rtp_audio.control_url = ctl_uri;
                    sdp_info.got_audio_control_uri = true;
                    logD_ (_self_func, "audio_control_url: ", self->rtp_audio.control_url);
                } else
                if (parsing_video) {
                    self->rtp_video.control_url = ctl_uri;
                    sdp_info.got_video_control_uri = true;
                    logD_ (_self_func, "video_control_url: ", self->rtp_video.control_url);
                }
            } else
            if (line.len() >= a_rtpmap.len() && equal (line.region (0, a_rtpmap.len()), a_rtpmap)) {
                parseSdpRtpmap (line, parsing_audio, parsing_video, &sdp_info);
            } else
            if (line.len() >= a_fmtp.len() && equal (line.region (0, a_fmtp.len()), a_fmtp)) {
                parseSdpFmtp (line, parsing_audio, parsing_video, &sdp_info);
            }
        }

        while (pos < mem.len() && (buf [pos] == '\r' || buf [pos] == '\n'))
            ++pos;

        line_beg = pos;
    }

    if (self->rtp_audio.enable) {
        if (   sdp_info.got_audio_control_uri
            && sdp_info.got_audio_payload_type
            && sdp_info.got_audio_is_aac
            && sdp_info.got_audio_aac_config)
        {
            self->audio_payload_type = sdp_info.audio_payload_type;

            if (sdp_info.aac_config.len()) {
                if (self->processAacConfig (sdp_info.aac_config)) {
                    self->rtp_audio.available = true;
                    self->audio_clock_rate = sdp_info.audio_clock_rate;
                    self->audio_channels   = sdp_info.audio_channels;
                } else {
                    logE (frames, _self_func, "processAacConfig() failed");
                }
            }
        }
    }

    if (self->rtp_video.enable) {
        logD_ (_self_func,
               "got_video_control_uri: ",  sdp_info.got_video_control_uri,  ", "
               "got_video_payload_type: ", sdp_info.got_video_payload_type, ", "
               "got_video_is_avc: ",       sdp_info.got_video_is_avc);

        if (   sdp_info.got_video_control_uri
            && sdp_info.got_video_payload_type
            && sdp_info.got_video_is_avc)
        {
            self->rtp_video.available = true;
            self->video_payload_type = sdp_info.video_payload_type;

            if (sdp_info.avc_parameter_sets.len()) {
                if (!self->processSdpParameterSets (sdp_info.avc_parameter_sets, sdp_info.avc_profile_level_id))
                    logE (frames, _self_func, "processSdpParameterSets() failed");
            }
        }
    }

    if (   !self->rtp_audio.available
        && !self->rtp_video.available)
    {
        logE_ (_self_func, "no media");
        self->close_pending = true;
        self->close_pending__disconnect_reason = DisconnectReason::ProtocolError;
        return;
    }

    self->nextStateAction ();
}

mt_mutex (mutex) Result
RtspClient::processSdpParameterSets (ConstMemory const parameter_sets,
                                     ConstMemory const profile_level_id)
{
    logD_ (_this_func, "ps: ", parameter_sets, ", pli: ", profile_level_id);

    Size set_beg = 0;
    Size pos = 0;
    while (pos <= parameter_sets.len()) {
        if (pos == parameter_sets.len() || parameter_sets.mem() [pos] == ',') {
            ConstMemory const set = parameter_sets.region (set_beg, pos - set_beg);
            logD_ (_this_func, "set: ", set);

            if (set.len() == 0)
                continue;

            {
                Byte * const decoded_buf = new (std::nothrow) Byte [base64DecodedBufferSize (set.len())];
                assert (decoded_buf);
                Size const decoded_len = base64Decode (set, decoded_buf);
                ConstMemory const decoded_mem (decoded_buf, decoded_len);

                h264_parser.parseOutOfBandParametersNal (decoded_mem);

                delete[] decoded_buf;
            }

            set_beg = pos + 1;
        }

        ++pos;
    }

    {
        Uint32 profile = 0;
        bool got_profile = false;
        if (profile_level_id.len()) {
            int base = 16;
            // Some buggy cameras send decimal profile-level-id, notably Pelco Sarix.
            if (profile_level_id.len() > 6)
                base = 10;

            if (strToUint32 (profile_level_id, &profile, NULL, base))
                got_profile = true;
            else
                logW_ (_this_func, "bad SDP profile_level_id: ", profile_level_id);
        }

      // Note: 'profile' is currently unused.
        (void) profile;
        (void) got_profile;
    }

    {
        PagePool::PageListHead page_list;
        Size msg_offs;
        Size msg_len;
        if (h264_parser.getAvcDecoderConfigurationRecord (&page_list, &msg_offs, &msg_len)) {
            VideoMessage video_msg;
            video_msg.codec_id    = VideoCodecId::AVC;
            video_msg.frame_type  = VideoFrameType::AvcSequenceHeader;
            video_msg.pts_nanosec = 0;
            video_msg.dts_nanosec = 0;
            video_msg.page_pool   = page_pool;
            video_msg.page_list   = page_list;
            video_msg.msg_len     = msg_len;
            video_msg.msg_offset  = msg_offs;

            // FIXME Firing with 'mutex' locked!
            stream->fireVideoMessage (&video_msg);

            page_pool->msgUnref (page_list.first, msg_len, msg_offs);
        }
    }

    // Note that we never fail.
    return Result::Success;
}

mt_mutex (mutex) Result
RtspClient::processAacConfig (ConstMemory const aac_config)
{
    logD_ (_this_func, "aac_config: ", aac_config);

    if (aac_config.len() < 1    ||
        aac_config.len() > 4096 ||
        aac_config.len() % 2)
    {
        logW_ (_this_func, "badd AAC config len: ", aac_config.len());
        return Result::Failure;
    }

    Byte aac_config_decoded [aac_config.len() / 2];

    for (Size i = 0; i < aac_config.len() - 1; i += 2) {
        Byte byte_str [2];
        byte_str [0] = aac_config.mem() [i + 0];
        byte_str [1] = aac_config.mem() [i + 1];

        Uint32 byte_value = 0;
        if (!strToUint32 (ConstMemory::forObject (byte_str), &byte_value, NULL, 16)) {
            logW_ (_this_func, "bad AAC config: ", aac_config);
            return Result::Failure;
        }

        aac_config_decoded [i >> 1] = (Byte) byte_value;
    }

    if (logLevelOn_ (LogLevel::Debug)) {
        logLock ();
        logD_locked_ (_this_func, "decoded aac config:");
        hexdump (logs, ConstMemory (aac_config_decoded, sizeof (aac_config_decoded)));
        logUnlock ();
    }

    {
        PagePool::PageListHead tmp_pages;
        page_pool->getFillPages (&tmp_pages, ConstMemory (aac_config_decoded, sizeof (aac_config_decoded)));

        AudioMessage audio_msg;
        audio_msg.codec_id = AudioCodecId::AAC;
        audio_msg.rate = audio_clock_rate;
        audio_msg.channels = audio_channels;

        audio_msg.frame_type = AudioFrameType::AacSequenceHeader;
        // TODO proper timestamp conversion, synchronization
        audio_msg.pts_nanosec = 0;
        audio_msg.dts_nanosec = 0;

        audio_msg.page_pool = page_pool;
        audio_msg.page_list = tmp_pages;
        audio_msg.msg_len = sizeof (aac_config_decoded);
        audio_msg.msg_offset = 0;

//#warning FIXME Firing with 'mutex' locked!
        stream->fireAudioMessage (&audio_msg);

        page_pool->msgUnref (tmp_pages.first);
    }

    return Result::Success;
}

mt_mutex (mutex) void
RtspClient::sendGetParameter ()
{
    last_method = "GET_PARAMETER";
    last_uri = newString (target_uri);

    MOMENT_SERVER__HEADERS_DATE
    conn_sender->send (page_pool,
                       true /* do_flush */,
                       "GET_PARAMETER ", target_uri, " RTSP/1.0\r\n"
                       "CSeq: ", rtsp_cseq, "\r\n",
                       getAuthHeader (),
                       (session_id ? makeString ("Session: ", session_id, "\r\n")->mem() : ConstMemory()),
                       "Date: ", ConstMemory (date_buf, date_len), "\r\n" \
                       "\r\n");
    ++rtsp_cseq;
}

void
RtspClient::pingTimerTick (void * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    logD_ (_self_func_);

    self->mutex.lock ();

    if (self->state < State::PlaySent) {
        self->mutex.unlock ();
        return;
    }

    self->sendGetParameter ();
    self->mutex.unlock ();
}

void
RtspClient::noVideoTimerTick (void * const _self)
{
    RtspClient * const self = static_cast <RtspClient*> (_self);

    Time const time_millisec = getTimeMilliseconds();

    self->mutex.lock ();

    if (self->state == State::Initial) {
        self->mutex.unlock ();
        return;
    }

    bool got_video = true;
    if (time_millisec >= self->last_frame_time_millisec) {
        if (self->no_avdata_timeout_millisec
            && time_millisec - self->last_frame_time_millisec >= self->no_avdata_timeout_millisec)
        {
            got_video = false;
        }
    } else {
        self->last_frame_time_millisec = time_millisec;
    }

    if (got_video) {
        if (self->got_first_frame) {
            self->mutex.unlock ();

            if (self->fetch_connection_frontend)
                self->fetch_connection_frontend.call (self->fetch_connection_frontend->gotVideo, true /* got_video */);
        } else {
            self->mutex.unlock ();
        }
    } else {
        self->mutex.unlock ();

        if (self->fetch_connection_frontend)
            self->fetch_connection_frontend.call (self->fetch_connection_frontend->gotVideo, false /* got_video */);
    }
}

Result
RtspClient::start ()
{
    logD_ (_this_func_);

    Uint16 tmp_audio_rtp_port  = 0;
    Uint16 tmp_audio_rtcp_port = 0;
    Uint16 tmp_video_rtcp_port = 0;
    Uint16 tmp_video_rtp_port  = 0;
    if (!tcp_interleaved) {
        if (rtp_audio.enable) {
            if (!audio_port_pair.openAndBindSocketPair (rtp_audio.rtp_socket,
                                                        rtp_audio.rtcp_socket,
                                                        &tmp_audio_rtp_port,
                                                        &tmp_audio_rtcp_port))
            {
                logE_ (_this_func, "openAndBindSocketPair() failed (audio)");
                return Result::Failure;
            }
        }

        if (rtp_video.enable) {
            if (!video_port_pair.openAndBindSocketPair (rtp_video.rtp_socket,
                                                        rtp_video.rtcp_socket,
                                                        &tmp_video_rtp_port,
                                                        &tmp_video_rtcp_port))
            {
                logE_ (_this_func, "openAndBindSocketPair() failed (video)");
                return Result::Failure;
            }
        }
    }

    if (!tcp_conn->open ()) {
        logE_ (_this_func, "tcp_conn.open() failed: ", exc->toString());
        return Result::Failure;
    }

    logD_ (_this_func, "RTP audio port: ", tmp_audio_rtp_port, ", RTCP audio port: ", tmp_audio_rtcp_port);
    logD_ (_this_func, "RTP video port: ", tmp_video_rtp_port, ", RTCP video port: ", tmp_video_rtcp_port);

    mutex.lock ();

    last_frame_time_millisec = getTimeMilliseconds();
    got_first_frame = false;

    rtp_audio.rtp_port  = tmp_audio_rtp_port;
    rtp_audio.rtcp_port = tmp_audio_rtcp_port;
    rtp_video.rtp_port  = tmp_video_rtp_port;
    rtp_video.rtcp_port = tmp_video_rtcp_port;

    if (!thread_ctx->getPollGroup()->addPollable_beforeConnect (tcp_conn->getPollable(), &pollable_key)) {
        logE_ (_this_func, "addPollable_beforeConnect() failed: ", exc->toString());
        mutex.unlock ();
        return Result::Failure;
    }

    TcpConnection::ConnectResult const connect_res = tcp_conn->connect (server_addr);
    if (connect_res == TcpConnection::ConnectResult_Error) {
        logE_ (_this_func, "tcp_conn.connect() failed: ", exc->toString());
        mutex.unlock ();
        return Result::Failure;
    }

    if (!thread_ctx->getPollGroup()->addPollable_afterConnect (tcp_conn->getPollable(), &pollable_key)) {
        logE_ (_this_func, "addPollable_afterConnect() failed: ", exc->toString());
        mutex.unlock ();
        return Result::Failure;
    }

    if (!tcp_interleaved) {
        if (rtp_audio.enable) {
            rtp_audio.rtp_pollable_key = thread_ctx->getPollGroup()->addPollable (rtp_audio.rtp_socket->getPollable());
            if (!rtp_audio.rtp_pollable_key) {
                logE_ (_this_func, "addPollable() failed (RTP audio): ", exc->toString());
                mutex.unlock ();
                return Result::Failure;
            }

            rtp_audio.rtcp_pollable_key = thread_ctx->getPollGroup()->addPollable (rtp_audio.rtcp_socket->getPollable());
            if (!rtp_audio.rtcp_pollable_key) {
                logE_ (_this_func, "addPollable() failed (RTCP audio): ", exc->toString());
                mutex.unlock ();
                return Result::Failure;
            }
        }

        if (rtp_video.enable) {
            rtp_video.rtp_pollable_key = thread_ctx->getPollGroup()->addPollable (rtp_video.rtp_socket->getPollable());
            if (!rtp_video.rtp_pollable_key) {
                logE_ (_this_func, "addPollable() failed (RTP video): ", exc->toString());
                mutex.unlock ();
                return Result::Failure;
            }

            rtp_video.rtcp_pollable_key = thread_ctx->getPollGroup()->addPollable (rtp_video.rtcp_socket->getPollable());
            if (!rtp_video.rtcp_pollable_key) {
                logE_ (_this_func, "addPollable() failed (RTCP video): ", exc->toString());
                mutex.unlock ();
                return Result::Failure;
            }
        }
    }

    if (connect_res == TcpConnection::ConnectResult_InProgress) {
        mutex.unlock ();
        return Result::Success;
    }
    assert (connect_res == TcpConnection::ConnectResult_Connected);

    doStart ();
    mutex.unlock ();

    return Result::Success;
}

mt_const void
RtspClient::rtpConnectionInit (RtpConnection       * const mt_nonnull rtp_conn,
                               ServerThreadContext * const mt_nonnull thread_ctx,
                               bool                  const enable)
{
    rtp_conn->enable = enable;

    rtp_conn->rtp_sender ->setConnection (rtp_conn->rtp_socket);
    rtp_conn->rtcp_sender->setConnection (rtp_conn->rtcp_socket);
    rtp_conn->rtp_sender ->setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    rtp_conn->rtcp_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());

    rtp_conn->rtp_receiver ->setFrontend (
            CbDesc<Receiver::Frontend> (
                    (rtp_conn->is_audio ? &audio_rtp_receiver_frontend  : &video_rtp_receiver_frontend ),
                    this,
                    this));
    rtp_conn->rtcp_receiver->setFrontend (
            CbDesc<Receiver::Frontend> (
                    (rtp_conn->is_audio ? &audio_rtcp_receiver_frontend : &video_rtcp_receiver_frontend),
                    this,
                    this));
    rtp_conn->rtp_receiver ->init (rtp_conn->rtp_socket,  thread_ctx->getDeferredProcessor());
    rtp_conn->rtcp_receiver->init (rtp_conn->rtcp_socket, thread_ctx->getDeferredProcessor());
}

mt_const Result
RtspClient::init (ServerThreadContext * const mt_nonnull thread_ctx,
                  PagePool            * const mt_nonnull page_pool,
                  ConstMemory          uri,
                  RtspClientConfig    * const mt_nonnull client_config,
                  MediaStream         * const mt_nonnull stream,
                  CbDesc<FetchConnection::FetchConnectionFrontend> const &fetch_connection_frontend)
{
  // TODO Asynchronous hostname resolution.

    RtspPeerDesc peer_desc;

    ConstMemory app_name;
    ConstMemory stream_name;
    bool        momentrtmp_proto = false;
    Size        loginpass_beg = 0;
    Size        loginpass_end = 0;
    if (!parseMomentUri (uri,
                         554 /* default_port */,
                         &peer_desc.server_addr,
                         &app_name,
                         &stream_name,
                         &momentrtmp_proto,
                         &peer_desc.login,
                         &peer_desc.password,
                         NULL /* ret_proto */,
                         &loginpass_beg,
                         &loginpass_end))
    {
        logE_ (_func, "could not parse uri: ", uri);
        return Result::Failure;
    }

    StRef<String> uri_no_loginpass;
    if (loginpass_beg && loginpass_end) {
        uri_no_loginpass = makeString (uri.region (0, loginpass_beg), uri.region (loginpass_end));
        uri = uri_no_loginpass;
    }

    peer_desc.uri = uri;

    return init_rtspPeerDesc (thread_ctx,
                              page_pool,
                              &peer_desc,
                              client_config,
                              stream,
                              fetch_connection_frontend);
}

mt_const Result
RtspClient::init_rtspPeerDesc (ServerThreadContext * const mt_nonnull thread_ctx,
                               PagePool            * const mt_nonnull page_pool,
                               RtspPeerDesc        * const mt_nonnull peer_desc,
                               RtspClientConfig    * const mt_nonnull client_config,
                               MediaStream         * const mt_nonnull stream,
                               CbDesc<FetchConnection::FetchConnectionFrontend> const &fetch_connection_frontend)
{
    logD_ (_this_func, "enable_audio: ", client_config->enable_audio, ", enable_video: ", client_config->enable_video);

    this->thread_ctx      = thread_ctx;
    this->page_pool       = page_pool;
    this->server_addr     = peer_desc->server_addr;
    this->target_uri      = newString (peer_desc->uri);
    this->login           = newString (peer_desc->login);
    this->password        = newString (peer_desc->password);
    this->tcp_interleaved = client_config->tcp_interleaved;
    this->stream          = stream;
    this->no_avdata_timeout_millisec = client_config->no_avdata_timeout_millisec;
    this->drop_sei_nal_units = client_config->drop_sei_nal_units;
    this->fetch_connection_frontend = fetch_connection_frontend;

    h264_parser.init (page_pool, &h264_callbacks, this);

    rtpConnectionInit (&rtp_audio, thread_ctx, client_config->enable_audio);
    rtpConnectionInit (&rtp_video, thread_ctx, client_config->enable_video);

    preassembly_buffer.init (1 << 17 /* 128 KB */ /* preassembly_limit */,
                             &preassembly_frontend,
                             this /* frontend_data */);

    tcp_conn->setFrontend (CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, this, this));

    conn_sender->setConnection (tcp_conn);
    conn_sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());

    conn_receiver->init (tcp_conn, thread_ctx->getDeferredProcessor());

    http_server->init (CbDesc<HttpServer::Frontend> (&http_server_frontend, this, this),
                       conn_receiver,
                       conn_sender,
                       thread_ctx->getDeferredProcessor(),
                       page_pool,
                       IpAddress() /* client_addr */,
                       false       /* client_mode */,
                       true        /* put_headers_to_hash */);

    if (client_config->no_avdata_check_interval_millisec) {
        thread_ctx->getTimers()->addTimer_microseconds (
                CbDesc<Timers::TimerCallback> (noVideoTimerTick, this, this),
                client_config->no_avdata_check_interval_millisec * 1000,
                true  /* periodical */,
                true  /* auto_delete */,
                false /* delete_after_tick */);
    }

    return Result::Success;
}

RtspClient::RtspClient (EmbedContainer * const embed_container)
    : FetchConnection (embed_container),

      tcp_interleaved (false),

      drop_sei_nal_units (false),

      no_avdata_timeout_millisec (0),
      last_frame_time_millisec   (0),
      got_first_frame            (false),

      rtp_audio (this /* rtsp_client */, true  /* is_audio */),
      rtp_video (this /* rtsp_client */, false /* is_audio */),

      tcp_conn        (this /* embed_container */),
      conn_sender     (this /* embed_container */),
      conn_receiver   (this /* embed_container */),
      http_server     (this /* embed_container */),

      closed (false),
      close_pending (false),
      close_pending__disconnect_reason (DisconnectReason::InvalidReason),

      audio_payload_type (0),
      video_payload_type (0),

      audio_clock_rate (44100),
      audio_channels   (1),

      rtsp_cseq       (1),
      /* Unused
      ssrc_audio      (0),
      ssrc_video      (0),
      */
      state           (State::Initial),
      got_describe_reply (false),
      data_state      (DataState::Header),
      channel_id      (0),
      msg_len         (0),
      got_len         (0),
      auth_required   (false),
      auth_digest     (false),
      audio_fragmented_len (0),
      video_fragmented_len (0),
      discard_video_fragment (false),
      expected_video_fragment_seq_num (0)
{
    logD_ (_this_func, "&tcp_conn: ", fmt_hex, (UintPtr) &tcp_conn);
}

// TODO releaseRtpConnection vs rtpConnectionRelease - ?
mt_const void
RtspClient::releaseRtpConnection (RtpConnection * const mt_nonnull rtp_conn)
{
    {
        RtpPacketList::iterator iter (rtp_conn->reordering_queue);
        while (!iter.done()) {
            RtpPacket * const rtp_packet = iter.next ();
            page_pool->msgUnref (rtp_packet->page_list.first);
            rtp_packet->page_list.reset ();
        }

        rtp_conn->reordering_queue.clear ();
    }
}

RtspClient::~RtspClient ()
{
    logD_ (_this_func_);

    mutex.lock ();

    if (ping_timer) {
        thread_ctx->getTimers()->deleteTimer (ping_timer);
        ping_timer = NULL;
    }

    releaseRtpConnection (&rtp_audio);
    releaseRtpConnection (&rtp_video);

    mt_unlocks (mutex) destroySelf (DisconnectReason::LocalClose);
}

}

