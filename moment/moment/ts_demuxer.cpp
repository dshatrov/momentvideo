#include <moment/util_h264.h>

#include <moment/ts_demuxer.h>


#define MOMENT__TS_DEMUXER__ADTS_HEADER_SIZE 7


namespace Moment {

static LogGroup libMary_logGroup_tsdemux ("tsdemux", LogLevel::I);

void
TsDemuxer::fillAndReportMediaMessage (MediaMessage * const mt_nonnull msg)
{
    msg->pts_nanosec = 0; // TODO
    msg->dts_nanosec = 0; // TODO

    msg->page_pool = page_pool;
//    msg->page_list  = ;
//    msg->msg_len    = ;
//    msg->msg_offset = ;
}

H264Parser::Callbacks const TsDemuxer::h264_callbacks = {
    h264_parameterSets,
    h264_accessUnit
};

void
TsDemuxer::h264_parameterSets (PagePool::PageListHead * const page_list,
                               Size                     const msg_offs,
                               Size                     const msg_len,
                               Time                     const pts,
                               Time                     const dts,
                               void                   * const _self)
{
    TsDemuxer * const self = static_cast <TsDemuxer*> (_self);

    logD (tsdemux, _self_func, "msg_len ", msg_len);

    if (!msg_len)
        return;

    Buffer buf;
    buf.allocate (msg_len);
    {
        PagePool::PageListArray pl_arr (page_list->first, msg_offs, msg_len);
        pl_arr.get (/*offset=*/ 0, buf.mem);
    }

    Ref<MediaDesc_AVC> media_desc__avc = grabNew <MediaDesc_AVC> ();
    media_desc__avc->init (/*width=*/     640,
                           /*height=*/    480,
                           /*framerate=*/  30,
                           buf.mem);

    MediaDescMessage mdesc_msg;
    mdesc_msg.pts_nanosec = pts;
    mdesc_msg.dts_nanosec = dts;

    mdesc_msg.page_pool  = self->page_pool;
    mdesc_msg.page_list  = *page_list;
    mdesc_msg.msg_offset = msg_offs;
    mdesc_msg.msg_len    = msg_len;

    mdesc_msg.media_desc__video = std::move (media_desc__avc);

    if (self->ts_handler)
        self->ts_handler->mediaMessage (&mdesc_msg, self->ts_handler_data);
}

void
TsDemuxer::h264_accessUnit (PagePool::PageListHead * const page_list,
                            Size                     const msg_offs,
                            Size                     const msg_len,
                            bool                     const is_keyframe,
                            Time                     const pts,
                            Time                     const dts,
                            void                   * const _self)
{
    TsDemuxer * const self = static_cast <TsDemuxer*> (_self);

    logD (tsdemux, _self_func, "msg_len ", msg_len);
//    PagePool::dumpPages (logs, page_list, msg_offs);

    VideoMessage video_msg;
    video_msg.pts_nanosec = pts;
    video_msg.dts_nanosec = dts;

    video_msg.page_pool  = self->page_pool;
    video_msg.page_list  = *page_list;
    video_msg.msg_offset = msg_offs;
    video_msg.msg_len    = msg_len;

    video_msg.frame_type = (is_keyframe ? VideoFrameType::KeyFrame : VideoFrameType::InterFrame);
    video_msg.codec_id   = VideoCodecId::AVC;

    if (self->ts_handler) {
        logS (tsdemux, _self_func, "msg_len ", msg_len);
//        dumpH264AvcNalUnits (page_list->first, msg_offs, msg_len);

        // TODO ignoring the result for now
        self->ts_handler->mediaMessage (&video_msg, self->ts_handler_data);
    }
}

void
TsDemuxer::processPesPacket (AccumulatedPacket * const mt_nonnull packet,
                             bool                const is_audio)
{
    logD (tsdemux, _this_func, "packet_len ", packet->packet_len, ", is_audio ", is_audio);

    if (!packet->packet_len) {
        goto _release_packet;
    }
    assert (packet->page_list.first);

  {
    Byte *buf = packet->page_list.first->getData();
    Size  len = packet->page_list.first->data_len;
    assert (len <= packet->packet_len);

    if (len < 6) {
        logD (tsdemux, _this_func, "PES packet is too short");
        goto _release_packet;
    }

  {
    Byte const packet_start_code_prefix [3] = { buf [0], buf [1], buf [2] };
    // logHex_ (LogLevel::Debug, ConstMemory::forObject (packet_start_code_prefix), _this_func, "packet_start_code_prefix:\n");

    Byte const stream_id = buf [3];
    Uint32 const pes_packet_length = ((Uint32) buf [4] << 8) | ((Uint32) buf [5]);

    logD (tsdemux, _this_func, "stream_id ", (Uint32) stream_id, ", pes_packet_length ", pes_packet_length);

    Size data_len = packet->packet_len;
    if (pes_packet_length != 0) {
        if (pes_packet_length + 6 > packet->packet_len) {
            logD (tsdemux, _this_func, "pes_packet_length ", pes_packet_length, " + 6 > packet_len ", packet->packet_len);
            goto _release_packet;
        }

        if (pes_packet_length + 6 < packet->packet_len)
            data_len = pes_packet_length + 6;
    }

    if (len < 9) {
        logD (tsdemux, _this_func, "PES packet is too short");
        goto _release_packet;
    }

    {
        Byte const pes_scrambling_control   = (buf [6] & 0x30) >> 4;
        bool const pes_priority             = (buf [6] & 0x08) >> 3;
        bool const data_alignment_indicator = (buf [6] & 0x04) >> 2;
        bool const copyright                = (buf [6] & 0x02) >> 1;
        bool const original_or_copy         = (buf [6] & 0x01);

        logD (tsdemux, _this_func, "pes_scrambling_control    ", pes_scrambling_control);
        logD (tsdemux, _this_func, "pes_priority              ", pes_priority);
        logD (tsdemux, _this_func, "data_alignment_indicator  ", data_alignment_indicator);
        logD (tsdemux, _this_func, "copyright                 ", copyright);
        logD (tsdemux, _this_func, "original_or_copy          ", original_or_copy);

        Byte const pts_dts_flags             = (buf [7] & 0xc0) >> 6;
        bool const escr_flag                 = (buf [7] & 0x20) >> 5;
        bool const es_rate_flag              = (buf [7] & 0x10) >> 4;
        bool const dsm_trick_mode_flag       = (buf [7] & 0x08) >> 3;
        bool const additional_copy_info_flag = (buf [7] & 0x04) >> 2;
        bool const pes_crc_flag              = (buf [7] & 0x20) >> 1;
        bool const pes_extension_flag        = (buf [7] & 0x01);

        logD (tsdemux, _this_func, "pts_dts_flags             ", pts_dts_flags);
        logD (tsdemux, _this_func, "escr_flag                 ", escr_flag);
        logD (tsdemux, _this_func, "es_rate_flag              ", es_rate_flag);
        logD (tsdemux, _this_func, "dsm_trick_mode_flag       ", dsm_trick_mode_flag);
        logD (tsdemux, _this_func, "additional_copy_info_flag ", additional_copy_info_flag);
        logD (tsdemux, _this_func, "pes_crc_flag              ", pes_crc_flag);
        logD (tsdemux, _this_func, "pes_extension_flag        ", pes_extension_flag);

        Uint32 const pes_header_data_length = buf [8];

        logD (tsdemux, _this_func, "pes_header_data_length    ", pes_header_data_length);

        if (   (pes_header_data_length + 9 > len)
            || (pes_header_data_length + 9 == len && packet->packet_len > len))
        {
            logD (tsdemux, _this_func, "pes_header_data_length ", pes_header_data_length, " + 9 >= len ", len);
            goto _release_packet;
        }

        Size const data_offs = pes_header_data_length + 9;
        data_len -= data_offs;

        logD (tsdemux, _this_func, "data_offs ", data_offs, ", data_len ", data_len);

        if (data_len == 0) {
            logD (tsdemux, _this_func, "no data");
            goto _release_packet;
        }

//        PagePool::dumpPages (logs, &packet->page_list, data_offs);

        if (is_audio) {
            if (data_len < MOMENT__TS_DEMUXER__ADTS_HEADER_SIZE) {
                logD_ ("ADTS AAC packet is too short");
                goto _release_packet;
            }

            assert (packet->page_list.first->data_len >= MOMENT__TS_DEMUXER__ADTS_HEADER_SIZE);

            /* TODO extract cdata from adts, compare to the old one, signal MediaDescMessage if changed */

            AudioMessage audio_msg;
            audio_msg.page_list  = packet->page_list;
            audio_msg.msg_offset = MOMENT__TS_DEMUXER__ADTS_HEADER_SIZE;
            audio_msg.msg_len    = data_len - MOMENT__TS_DEMUXER__ADTS_HEADER_SIZE;

            audio_msg.frame_type = AudioFrameType::RawData;
            audio_msg.codec_id   = AudioCodecId::AAC;
            audio_msg.rate       = 44100 /* TODO extract from cdata/adts header */;
            audio_msg.channels   = 1     /* TODO extract from cdata/adts header */;

            fillAndReportMediaMessage (&audio_msg);
        } else {
            // TODO support for unaligned byte streams in H264Parser
            h264_parser.parseByteStreamNals (packet->page_list.first,
                                             data_offs,
                                             data_len,
                                             /*TODO*/ /*pts=*/ 0,
                                             /*TODO*/ /*dts=*/ 0);
            // Note: AU alignment is specific to moment.
            h264_parser.forceAccessUnitEnd ();
        }
    }
  }
  }

_release_packet:
    page_pool->msgUnref (packet->page_list.first);
    packet->page_list.reset();
    packet->packet_len = 0;
}

Result
TsDemuxer::processTsPacket (ConstMemory const mem)
{
    Byte const * const buf = mem.buf();
    Size         const len = mem.len();

    assert (len == 188);

    logD (tsdemux, _this_func, "len ", len);
//    hexdump (logs, mem);

    Byte const sync_byte = buf [0];
    if (sync_byte != 0x47)
        logD (tsdemux, _this_func, "sync_byte != 0x47");

    bool const transport_error_indicator    = buf [1] & 0x80;
    bool const payload_unit_start_indicator = buf [1] & 0x40;
    bool const transport_priority           = buf [1] & 0x20;

    Uint16 const pid =   ((Uint16) (buf [1] & 0x1f) << 8)
                       | ((Uint16)  buf [2]);

    Byte const transport_scrambling_control = (buf [3] & 0xc0) >> 6;
    Byte const adaptation_field_control     = (buf [3] & 0x30) >> 4;
    Byte const continuity_counter           =  buf [3] & 0x0f;

    logD (tsdemux, _this_func, "sync_byte                    0x", fmt_hex, sync_byte);
    logD (tsdemux, _this_func, "transport_error_indicator    ", transport_error_indicator);
    logD (tsdemux, _this_func, "payload_unit_start_indicator ", payload_unit_start_indicator);
    logD (tsdemux, _this_func, "transport_priority           ", transport_priority);
    logD (tsdemux, _this_func, "pid                          ", pid);
    logD (tsdemux, _this_func, "transport_scrambling_control ", transport_scrambling_control);
    logD (tsdemux, _this_func, "adaptation_field_control     ", adaptation_field_control);
    logD (tsdemux, _this_func, "continuity_counter           ", continuity_counter);

    Size pos = 4;
    if (adaptation_field_control == 2 || adaptation_field_control == 3) {
        Byte const adaptation_field_length = buf [4];
        ++pos;

        logD (tsdemux, _this_func, "adaptation_field_length      ", adaptation_field_length);

#warning TODO check length
        if (adaptation_field_length > 0) {
            bool const discontinuity_indicator              = buf [5] & 0x80;
            bool const random_access_indicator              = buf [5] & 0x40;
            bool const elementary_stream_priority_indicator = buf [5] & 0x20;
            bool const PCR_flag                             = buf [5] & 0x10;
            bool const OPCR_flag                            = buf [5] & 0x08;
            bool const splicing_point_flag                  = buf [5] & 0x04;
            bool const transport_private_data_flag          = buf [5] & 0x02;
            bool const adaptation_field_extension_flag      = buf [5] & 0x01;
            ++pos;

            logD (tsdemux, _this_func, "discontinuity_indicator      ", discontinuity_indicator);
            logD (tsdemux, _this_func, "random_access_indicator      ", random_access_indicator);
            logD (tsdemux, _this_func, "elementary_stream_priority_indicator ", elementary_stream_priority_indicator);
            logD (tsdemux, _this_func, "PCR_flag                     ", PCR_flag);
            logD (tsdemux, _this_func, "OPCR_flag                    ", OPCR_flag);
            logD (tsdemux, _this_func, "splicing_point_flag          ", splicing_point_flag);
            logD (tsdemux, _this_func, "transport_private_data_flag  ", transport_private_data_flag);
            logD (tsdemux, _this_func, "adaptation_field_extension_flag ", adaptation_field_extension_flag);

            if (PCR_flag) {
                Uint64 const program_clock_reference_base =    ((Uint64)  buf [ 6] << 25)
                                                            |  ((Uint64)  buf [ 7] << 17)
                                                            |  ((Uint64)  buf [ 8] <<  9)
                                                            |  ((Uint64)  buf [ 9] <<  1)
                                                            | (((Uint64) (buf [10] & 0x80)) >> 7);

                Uint16 const program_clock_reference_extension =   ((Uint64) (buf [10] & 0x1) << 9)
                                                                 |  (Uint64)  buf [11];

                pos += 6;

                logD (tsdemux, _this_func, "program_clock_reference_base ", program_clock_reference_base);
                logD (tsdemux, _this_func, "program_clock_reference_extension ", program_clock_reference_extension);
            }

            if (OPCR_flag) {
                Uint64 const original_program_clock_reference_base =    ((Uint64)  buf [pos    ] << 25)
                                                                     |  ((Uint64)  buf [pos + 1] << 17)
                                                                     |  ((Uint64)  buf [pos + 2] <<  9)
                                                                     |  ((Uint64)  buf [pos + 3] <<  1)
                                                                     | (((Uint64) (buf [pos + 4] & 0x80)) >> 7);

                Uint16 const original_program_clock_reference_extension =   ((Uint64) (buf [pos + 4] & 0x1) << 9)
                                                                          |  (Uint64)  buf [pos + 5];

                pos += 6;

                logD (tsdemux, _this_func, "original_program_clock_reference_base ", original_program_clock_reference_base);
                logD (tsdemux, _this_func, "original_program_clock_reference_extension ", original_program_clock_reference_extension);
            }

            if (splicing_point_flag) {
                Byte const splice_countdown = buf [pos];
                ++pos;

                logD (tsdemux, _this_func, "splice_countdown ", splice_countdown);
            }

            if (transport_private_data_flag) {
#warning TODO check length
                Byte const transport_private_data_length = buf [pos];
                ++pos;

                logD (tsdemux, _this_func, "transport_private_data_length ", transport_private_data_length);
//                hexdump (logs, ConstMemory (buf + pos, transport_private_data_length));

                pos += transport_private_data_length;
            }

            // ignoring adaptation_field_extension_flag
        }

        pos = 5 + adaptation_field_length;
    }

    if (adaptation_field_control != 1 && adaptation_field_control != 3) {
        // no payload
        logD (tsdemux, _this_func, "no payload");
        return Result::Success;
    }
    assert (pos <= 188);

    Byte const data_len = 188 - pos;
    logD (tsdemux, _this_func, "data_len ", data_len);

    // TODO 1. prepend data to current pes packet;
    //      2. watch out for pes packet size limit;
    //      3. if payload_unit_start_indicator, then process pes packet
    //      4. unpack avc to MediaMessage format

    AccumulatedPacket *dst_packet = NULL;
    if (pid == 0x40)
        dst_packet = &audio_pes_packet;
    else
    if (pid == 0x41)
        dst_packet = &video_pes_packet;

    if (!dst_packet) {
        logD (tsdemux, _this_func, "!dst_packet");
        return Result::Success;
    }

    if (payload_unit_start_indicator && dst_packet->packet_len)
        processPesPacket (dst_packet, (dst_packet == &audio_pes_packet));
    else
        logD (tsdemux, _this_func, "no full pes packet yet, dst_packet->packet_len ", dst_packet->packet_len);

    logD (tsdemux, _this_func, "data_len ", data_len);
//    hexdump (logs, ConstMemory (buf + pos, data_len));

    page_pool->getFillPages (&dst_packet->page_list, ConstMemory (buf + pos, data_len));
    dst_packet->packet_len += data_len;

    return Result::Success;
}

Result
TsDemuxer::processData (ConstMemory   const mem,
                        Size        * const mt_nonnull ret_accepted)
{
    Byte const *buf = mem.buf();
    Size        len = mem.len();

    Size const ts_packet_size = 188;

    while (len >= ts_packet_size) {
        processTsPacket (ConstMemory (buf, ts_packet_size));

        buf += ts_packet_size;
        len -= ts_packet_size;
    }

    if (audio_pes_packet.packet_len)
        processPesPacket (&audio_pes_packet, /*is_audio=*/ true);
    if (video_pes_packet.packet_len)
        processPesPacket (&video_pes_packet, /*is_audio=*/ false);

    *ret_accepted = mem.len() - len;
    return Result::Success;
}

void
TsDemuxer::init (PagePool * const mt_nonnull page_pool)
{
    logD (tsdemux, _this_func, "h264_parser " _ptr (&h264_parser));

    this->page_pool = page_pool;
    h264_parser.init (page_pool, &h264_callbacks, this);
}

TsDemuxer::~TsDemuxer ()
{
    page_pool->msgUnref (audio_pes_packet.page_list.first);
    page_pool->msgUnref (video_pes_packet.page_list.first);
}

}

