/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/ts_muxer.h>


#define MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE 188

#define MOMENT__TS_MUXER__PMT_PID   0x20
#define MOMENT__TS_MUXER__AUDIO_PID 0x40
#define MOMENT__TS_MUXER__VIDEO_PID 0x41

#define MOMENT__TS_MUXER__STREAM_ID__AAC 0xc0
#define MOMENT__TS_MUXER__STREAM_ID__AVC 0xe0

#define MOMENT__TS_MUXER__STREAM_TYPE__AAC 0x0f
#define MOMENT__TS_MUXER__STREAM_TYPE__AVC 0x1b

#define MOMENT__TS_MUXER__PROGRAM_NUMBER 1

#define MOMENT__TS_MUXER__PAYLOAD_UNIT_START_INDICATOR 0x4000

#define MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD          0x10
#define MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__ADAPTATION_FIELD 0x20

#define MOMENT__TS_MUXER__PCR_FLAG 0x10

#define MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS     14
#define MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS_DTS 19

#define MOMENT__TS_MUXER__ADTS_HEADER_SIZE 7


namespace Moment {

static LogGroup libMary_logGroup_tsmux ("tsmux", LogLevel::I);

static Uint32 crc_tab [256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static Uint32
crc32 (Byte * const mt_nonnull buf,
       Size   const len)
{
    Uint32 crc = 0xffffffff;
    for (Size i = 0; i < len; ++i)
        crc = (crc << 8) ^ crc_tab [((crc >> 24) ^ buf [i]) & 0xff];

    return crc;
}

static void write_ts (Byte   * const mt_nonnull buf,
                      Byte     const head,
                      Uint64   const ts)
{
    buf [0] = 0xff & (  (head << 4)
                      | ((ts >> 29) & 0x0e) // *TS [32...30]
                      | 0x01);              // marker_bit

    writeBe16 (buf + 1,
               (  ((ts >> 14) & 0xfffe)     // *TS [29...15]
                | 0x01));                   // marker_bit

    writeBe16 (buf + 3,
               (  ((ts << 1) & 0xfffe)      // *TS [14...0]
                | 0x01));                   // marker_bit
}

class TsMuxer::TsFiller
{
  private:
    TsMuxer * const ts_muxer;
    Byte      const pid;
    Byte    * const continuity_counter;

    Size payload_bytes_available;

    Byte *packet_buf;
    Size  packet_len;

  public:
    void addData (ConstMemory const mem)
    {
        Byte const *buf = mem.buf();
        Size        len = mem.len();
        for (;;) {
            if (len > packet_len) {
                memcpy (packet_buf, buf, packet_len);
                buf += packet_len;
                len -= packet_len;
                payload_bytes_available -= packet_len;

                packet_buf = ts_muxer->allocateTransportPacket ();
                packet_buf = fillTsHeader (packet_buf,
                                           0 /* payload_unit_start_indicator */,
                                           pid,
                                           continuity_counter,
                                           MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD,
                                           0 /* pcr */,
                                           payload_bytes_available,
                                           0 /* adaptation_field_flags */,
                                           &packet_len);
            } else {
                memcpy (packet_buf, buf, len);
                packet_buf += len;
                packet_len -= len;
                payload_bytes_available -= len;
                break;
            }
        }
    }

    void addDataFromArray (Array * const mt_nonnull arr,
                           Size   offset,
                           Size   len)
    {
        for (;;) {
            if (len > packet_len) {
                arr->get (offset, Memory (packet_buf, packet_len));
                offset += packet_len;
                len    -= packet_len;
                payload_bytes_available -= packet_len;

                packet_buf = ts_muxer->allocateTransportPacket ();
                packet_buf = fillTsHeader (packet_buf,
                                           0 /* payload_unit_start_indicator */,
                                           pid,
                                           continuity_counter,
                                           MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD,
                                           0 /* pcr */,
                                           payload_bytes_available,
                                           0 /* adaptation_field_flags */,
                                           &packet_len);
            } else {
                arr->get (offset, Memory (packet_buf, len));
                packet_buf += len;
                packet_len -= len;
                payload_bytes_available -= len;
                break;
            }
        }
    }

    TsFiller (TsMuxer * const mt_nonnull ts_muxer,
              Byte      const pid,
              Byte    * const mt_nonnull continuity_counter,
              Size      const payload_bytes_available,
              Byte    * const mt_nonnull packet_buf,
              Size      const packet_len)
        : ts_muxer                (ts_muxer),
          pid                     (pid),
          continuity_counter      (continuity_counter),
          payload_bytes_available (payload_bytes_available),
          packet_buf              (packet_buf),
          packet_len              (packet_len)
    {}
};

Byte*
TsMuxer::allocateTransportPacket ()
{
    {
        Size const page_size = page_pool->getPageSize();
        assert (page_size >= MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE);

        PagePool::Page * const page = page_list.last;
        if (mt_likely (page)) {
            assert (page->data_len <= page_size);
            if (mt_likely (page_size - page->data_len >= MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE)) {
                Byte * const packet_buf = page->getData() + page->data_len;
                page->data_len += MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE;

                total_data_len += MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE;
                return packet_buf;
            }
        }
    }

    PagePool::Page * const new_page = page_pool->grabPage ();
    page_list.appendPages (new_page);
    new_page->data_len = MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE;

    total_data_len += MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE;
    return new_page->getData();
}

Byte*
TsMuxer::fillTsHeader (Byte   * const mt_nonnull buf,
                       Uint16   const payload_unit_start_indicator,
                       Uint16   const pid,
                       Uint8  * const mt_nonnull continuity_counter,
                       Uint8    const adaptation_field_control,
                       Time     const pcr,
                       Size     const payload_bytes_available,
                       Uint8    const adaptation_field_flags,
                       Size   * const mt_nonnull ret_payload_len)
{
    // 8 bits: sync byte = 0x47
    buf [0] = 0x47;

    //  1 bit : transport_error_endicator = 0
    //  1 bit : payload_unit_start_indicator
    //  1 bit : transport_priority = 0
    // 13 bits: PID
    writeBe16 (buf + 1, payload_unit_start_indicator | pid);

    //  2 bits: transport_scrambling_control = 0
    //  2 bits: adaptation_field_control
    //  4 bits: continuity_counter
    buf [3] = (*continuity_counter & 0x0f) | adaptation_field_control;

    Size pos = 4;
    if (   (adaptation_field_control & MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__ADAPTATION_FIELD)
        || payload_bytes_available < MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE - 4)
    {
        buf [3] |= MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__ADAPTATION_FIELD;

        // 1 bit: discontinuity_indicator
        // 1 bit: random_access_indicator
        // 1 bit: elementary_stream_priority_indicator
        // 1 bit: PCR_flag
        // 1 bit: OPCR_flag
        // 1 bit: splicing_point_flag
        // 1 bit: transport_private_data_flag
        // 1 bit: adaptation_field_extension_flag
        //
        buf [5] = adaptation_field_flags;
        pos += 2 /* adaptation_field_length + flags */;

        if (adaptation_field_flags & MOMENT__TS_MUXER__PCR_FLAG) {
            // 33 bits: program_clock_reference_base
            //  6 bits: reserved
            //  9 bits: program_clock_reference_extension
            buf [ 6] =  (pcr >> 25) & 0xff;
            buf [ 7] =  (pcr >> 17) & 0xff;
            buf [ 8] =  (pcr >>  9) & 0xff;
            buf [ 9] =  (pcr >>  1) & 0xff;
            buf [10] = ((pcr <<  7) & 0x80) | 0x7e;
            buf [11] = 0;
            pos += 6;
        }

        Size const free_space = MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE - pos;
        if (payload_bytes_available < free_space) {
            Size const min_pos = MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE - payload_bytes_available;
            if (pos < min_pos) {
                // stuffing_byte
                memset (buf + pos, 0xff, min_pos - pos);
                pos = min_pos;
            }

            *ret_payload_len = payload_bytes_available;
        } else {
            *ret_payload_len = free_space;
        }

        // 8 bits: adaptation_field_length
        buf [4] = pos - 5;
    } else {
        *ret_payload_len = MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE - 4;
    }

    if (adaptation_field_control & MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD)
        ++*continuity_counter;

    return buf + pos;
}

void
TsMuxer::fillPesHeader (Byte   * const mt_nonnull buf,
                        Byte     const stream_id,
                        Uint64   const pts,
                        Uint64   const dts,
                        Size     const pes_data_len,
                        Size   * const mt_nonnull ret_len)
{
    // 24 bits: packet_start_code_prefix
    buf [0] = 0;
    buf [1] = 0;
    buf [2] = 1;

    // 8 bits: stream id
    buf [3] = stream_id;

    // 16 bits: PES_packet_length (filled below)

    // 2 bits: 10
    // 2 bits: PES_scrambling_control = 00
    // 1 bit : PES_priority = 0
    // 1 bit : data_alignment_indicator = 0
    // 1 bit : copyright = 0
    // 1 bit : original_copy = 1
    buf [6] = 0x81;

    Byte pes_header_data_len;
    Byte pts_dts_flags;
    if (pts == dts) {
        pts_dts_flags = 0x80;
        pes_header_data_len = 5;
    } else {
        pts_dts_flags = 0xc0;
        pes_header_data_len = 10;
    }

    // 2 bits: PTS_DTS_flags = 10
    // 1 bit : ESCR flag = 0
    // 1 bit : ES_rate_flag = 0
    // 1 bit : DSM_trick_mode_flag = 0
    // 1 bit : additional_copy_info_flag = 0
    // 1 bit : PES_CRC_flag = 0
    // 1 bit : PES_extension_flag = 0
    buf [7] = pts_dts_flags;

    // 8 bits: PES_header_data_length
    buf [8] = pes_header_data_len;

    if (pts == dts) {
        write_ts (buf +  9, 0x02, pts);
    } else {
        write_ts (buf +  9, 0x03, pts);
        write_ts (buf + 14, 0x01, dts);
    }

    // PES_packet_length
    if (stream_id == MOMENT__TS_MUXER__STREAM_ID__AVC)
        writeBe16 (buf + 4, 0);
    else
        writeBe16 (buf + 4, 9 + pes_header_data_len + pes_data_len);

    *ret_len = 9 + pes_header_data_len;
}

// aac_cdata_len must be at least 2
// @adts_header must be 7 bytes long
void
TsMuxer::fillAdtsAacHeader (AudioMessage * const mt_nonnull audio_msg,
                            Byte         * const mt_nonnull adts_header)
{
    unsigned char tmp_buf [2];
    {
        PagePool::PageListArray codec_data_arr (aac_cdata_page_list.first,
                                                aac_cdata_offset,
                                                aac_cdata_len);
        codec_data_arr.get (0 /* offset */, Memory (tmp_buf, 2));
    }

    Size const out_len = audio_msg->msg_len + 7;

    // sync point over a full byte
    adts_header[0] = 0xff;

    // sync point continued over first 4 bits + static 4 bits
    // (ID, layer, protection)
    //
    // TODO ID is MPEG version. 0 for MPEG-4, 1 for MPEG-2.
    //      We set ID to 0 unconditionally. The "1" is for protection_absent.
    adts_header[1] = 0xf1;

    {
        // Note: For reference, see http://wiki.multimedia.cx/index.php?title=ADTS
        //
        // Note: gst-plugins-bad/gst/mpegtsmux/mpegtsmux_aac.c and
        //       gst-plugins-bad/gst/mpegpsmux/mpegpsmux_aac.c
        //       contain a bug: wrong bits taken for obj_type,
        //                      and obj_type++ there is incorrect.
        //
        Byte const obj_type = (((tmp_buf [0] & 0xf8) >> 3) & 0x3) - (1 /* for adts header */);


        // object type over first 2 bits
        adts_header[2] = obj_type << 6;
    }

    {
        Byte const rate_idx =   ((tmp_buf [0] & 0x03) << 1)
                              | ((tmp_buf [1] & 0x80) >> 7);

        // rate index over next 4 bits
        adts_header[2] |= (rate_idx << 2);
    }

    {
        Byte const channels  = (tmp_buf [1] & 0x78) >> 3;

        // channels over last 2 bits
        adts_header[2] |= (channels & 0x4) >> 2;
        // channels continued over next 2 bits + 4 bits at zero
        adts_header[3] = (channels & 0x3) << 6;
    }

    // frame size over last 2 bits
    adts_header[3] |= (out_len & 0x1800) >> 11;
    // frame size continued over full byte
    adts_header[4] = (out_len & 0x1ff8) >> 3;
    // frame size continued first 3 bits
    adts_header[5] = (out_len & 0x7) << 5;
    // buffer fullness (0x7ff for VBR) over 5 last bits
    adts_header[5] |= 0x1f;
    // buffer fullness (0x7ff for VBR) continued over 6 first bits + 2 zeros for
    // number of raw data blocks
    adts_header[6] = 0xfc;
}

Result
TsMuxer::muxAvcCodecData (Array                   * const mt_nonnull arr,
                          Size                      const arr_len,
                          PagePool               ** const mt_nonnull ret_page_pool,
                          PagePool::PageListHead  * const mt_nonnull ret_page_list,
                          Size                    * const ret_msg_len,
                          Byte                    * const ret_nal_length_size)
{
    *ret_msg_len         = 0;
    *ret_nal_length_size = 0;
    *ret_page_pool       = page_pool;

    unsigned char tmp_buf [4];

    Byte const startcode [4] = { 0x00, 0x00, 0x00, 0x01 };
    ConstMemory const startcode_mem = ConstMemory::forObject (startcode);

    Size offset = 4;
    Size i      = 0;
    Size nb_sps = 0;
    Size nb_pps = 0;

    {
      /* Get NAL length size */
      if (arr_len < offset + 1) {
          logD (tsmux, _func, "No NAL length size");
          goto _failure;
      }
      arr->get (offset, Memory (tmp_buf, 1));
      *ret_nal_length_size = (tmp_buf [0] & 0x03) + 1;
      offset++;
      logD (tsmux, _func, "NAL length size: ", *ret_nal_length_size);

      if (*ret_nal_length_size == 3) {
        // forbidden value
          *ret_nal_length_size = 0;
      }

      /* How many SPS */
      if (arr_len < offset + 1) {
          logD (tsmux, _func, "No SPS count");
          goto _failure;
      }
      arr->get (offset, Memory (tmp_buf, 1));
      nb_sps = tmp_buf [0] & 0x1f;
      offset++;
      logD (tsmux, _func, "SPS count: ", nb_sps);

      /* For each SPS */
      for (i = 0; i < nb_sps; i++) {
          logD (tsmux, _func, "SPS iteration");

          if (arr_len < offset + 2) {
              logD (tsmux, _func, "No SPS size");
              goto _failure;
          }
          arr->get (offset, Memory (tmp_buf, 2));
          Uint16 sps_size = readBe16 (tmp_buf);
          /* Jump over SPS size */
          offset += 2;
          logD (tsmux, _func, "SPS size: ", sps_size);

          /* Fake a start code */
          page_pool->getFillPages (ret_page_list, startcode_mem);
          *ret_msg_len += startcode_mem.len();

          /* Now push the SPS */
          if (arr_len < offset + sps_size) {
              logD (tsmux, _func, "No SPS body");
              goto _failure;
          }
          logD (tsmux, _func, "arr_len: ", arr_len, ", offset: ", offset, ", sps_size: ", sps_size);

          page_pool->getFillPagesFromArray (ret_page_list, arr, offset, sps_size);

          *ret_msg_len += sps_size;
          offset += sps_size;
      }

      /* How many PPS */
      if (arr_len < offset + 1) {
          logD (tsmux, _func, "No PPS count");
          goto _failure;
      }
      arr->get (offset, Memory (tmp_buf, 1));
      nb_pps = tmp_buf [0];
      offset++;
      logD (tsmux, _func, "PPS count: ", nb_pps);

      /* For each PPS */
      for (i = 0; i < nb_pps; i++) {
          logD (tsmux, _func, "PPS iteration");

          if (arr_len < offset + 2) {
              logD (tsmux, _func, "No PPS size");
              goto _failure;
          }
          arr->get (offset, Memory (tmp_buf, 2));
          Uint16 pps_size = readBe16 (tmp_buf);
          /* Jump over PPS size */
          offset += 2;
          logD (tsmux, _func, "PPS size: ", pps_size);

          /* Fake a start code */
          page_pool->getFillPages (ret_page_list, startcode_mem);
          *ret_msg_len += startcode_mem.len();

          /* Now push the PPS */
          if (arr_len < offset + pps_size) {
              logD (tsmux, _func, "No PPS body");
              goto _failure;
          }
          logD (tsmux, _func, "offset: ", offset);

          page_pool->getFillPagesFromArray (ret_page_list, arr, offset, pps_size);

          *ret_msg_len += pps_size;
          offset += pps_size;
      }
    }

    logD (tsmux, _func, "Success");
    return Result::Success;

  _failure:
    logD (tsmux, _func, "Failure");
    return Result::Failure;
}

void
TsMuxer::muxMediaMessage (MediaMessage * const mt_nonnull msg)
{
    logS (tsmux, *msg);

    if (   msg->msg_type == MediaMessage::Type_Audio
        || msg->msg_type == MediaMessage::Type_Video)
    {
        if (msg->msg_type == MediaMessage::Type_Audio) {
            if (mt_unlikely (!got_audio))
                return;

            AudioMessage * const audio_msg = static_cast <AudioMessage*> (msg);
            if (mt_unlikely (audio_msg->codec_id != AudioCodecId::AAC))
                return;

            muxAacMessage (audio_msg);
        } else {
            if (mt_unlikely (!got_video))
                return;

            VideoMessage * const video_msg = static_cast <VideoMessage*> (msg);
            if (mt_unlikely (video_msg->codec_id != VideoCodecId::AVC))
                return;

            muxAvcMessage (video_msg);
        }
    } else
    if (msg->msg_type == MediaMessage::Type_MediaDesc) {
        MediaDescMessage * const media_desc_msg = static_cast <MediaDescMessage*> (msg);

        if (media_desc_msg->media_desc__audio) {
            if (mt_unlikely (!got_audio))
                return;

            if (mt_unlikely (media_desc_msg->media_desc__audio->getType() != MediaDesc_Audio::AAC))
                return;

            MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (media_desc_msg->media_desc__audio.ptr());
            muxAacMediaDesc (media_desc__aac);
        }

        if (media_desc_msg->media_desc__video) {
            if (mt_unlikely (!got_video))
                return;

            if (mt_unlikely (media_desc_msg->media_desc__video->getType() != MediaDesc_Video::AVC))
                return;

            MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (media_desc_msg->media_desc__video.ptr());
            muxAvcMediaDesc (media_desc__avc);
        }
    }
}

void
TsMuxer::muxAacMediaDesc (MediaDesc_AAC * const mt_nonnull media_desc__aac)
{
    logS (tsmux, *media_desc__aac);

    if (aac_cdata_page_pool) {
        aac_cdata_page_pool->msgUnref (aac_cdata_page_list.first);
        aac_cdata_page_list.reset ();
        aac_cdata_page_pool = NULL;
    }

    ConstMemory const aac_cdata_mem = media_desc__aac->getCodecData();

    page_pool->getFillPages (&aac_cdata_page_list, aac_cdata_mem);
    aac_cdata_page_pool = page_pool;
    aac_cdata_offset    = 0;
    aac_cdata_len       = aac_cdata_mem.len();
}

void
TsMuxer::muxAacMessage (AudioMessage * const mt_nonnull audio_msg)
{
    logS (tsmux, *audio_msg);

    if (mt_unlikely (audio_msg->frame_type == AudioFrameType::AacSequenceHeader)) {
        logS (tsmux, "AacSequenceHeader");

        if (aac_cdata_page_pool)
            aac_cdata_page_pool->msgUnref (aac_cdata_page_list.first);

        aac_cdata_page_pool = audio_msg->page_pool;
        aac_cdata_page_list = audio_msg->page_list;
        aac_cdata_page_pool->msgRef (aac_cdata_page_list.first);

        aac_cdata_offset = audio_msg->msg_offset;
        aac_cdata_len    = audio_msg->msg_len;
        return;
    }

    if (mt_unlikely (!audio_msg->frame_type.isAudioData())) {
        logS (tsmux, "not an audio data message");
        return;
    }

    // TODO Can valid AAC messages be empty? (likely not)
    if (mt_unlikely (!audio_msg->msg_len)) {
        logS (tsmux, "empty AAC message");
        return;
    }

    if (mt_unlikely (aac_cdata_len < 2)) {
        logS (tsmux, "AAC codec data is too short: aac_cdata_len ", aac_cdata_len);
        return;
    }

    Size payload_bytes_available =   MOMENT__TS_MUXER__ADTS_HEADER_SIZE
                                   + MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS
                                   + audio_msg->msg_len;
    Byte *packet_buf = allocateTransportPacket ();

    // TODO Better integer scaling?
    Uint64 const pts = audio_msg->pts_nanosec * 9 / 100000;

    Size packet_len = 0;
    packet_buf = fillTsHeader (packet_buf,
                               MOMENT__TS_MUXER__PAYLOAD_UNIT_START_INDICATOR,
                               MOMENT__TS_MUXER__AUDIO_PID,
                               &audio_continuity_counter,
                               (  MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD
                                | MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__ADAPTATION_FIELD),
                               pts /* pcr */,
                               payload_bytes_available,
                               MOMENT__TS_MUXER__PCR_FLAG,
                               &packet_len);

    {
        Size pes_header_len = 0;
        fillPesHeader (packet_buf,
                       MOMENT__TS_MUXER__STREAM_ID__AAC,
                       pts,
                       pts  /* for audio, dts == pts */,
                       MOMENT__TS_MUXER__ADTS_HEADER_SIZE + audio_msg->msg_len,
                       &pes_header_len);
        assert (pes_header_len == MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS);

        packet_buf += MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS;
        packet_len -= MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS;
        payload_bytes_available -= MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS;
    }

    assert (packet_len >= MOMENT__TS_MUXER__ADTS_HEADER_SIZE);
    fillAdtsAacHeader (audio_msg, packet_buf);
    packet_buf += MOMENT__TS_MUXER__ADTS_HEADER_SIZE;
    packet_len -= MOMENT__TS_MUXER__ADTS_HEADER_SIZE;
    payload_bytes_available -= MOMENT__TS_MUXER__ADTS_HEADER_SIZE;

    {
        TsFiller ts_filler (this /* ts_muxer */,
                            MOMENT__TS_MUXER__AUDIO_PID,
                            &audio_continuity_counter,
                            payload_bytes_available,
                            packet_buf,
                            packet_len);

        PagePool::PageListArray arr (audio_msg->page_list.first,
                                     audio_msg->msg_offset,
                                     audio_msg->msg_len);

        ts_filler.addDataFromArray (&arr, 0 /* offset */, audio_msg->msg_len);
    }
}

void
TsMuxer::muxAvcMediaDesc (MediaDesc_AVC * const mt_nonnull media_desc__avc)
{
    if (avc_cdata_page_pool) {
        avc_cdata_page_pool->msgUnref (avc_cdata_page_list.first);
        avc_cdata_page_list.reset ();
        avc_cdata_page_pool = NULL;
    }

    ConstMemory const avc_cdata = media_desc__avc->getCodecData ();
    if (mt_unlikely (!avc_cdata.len()))
        return;

    ConstMemoryArray arr (avc_cdata);

    PagePool               *tmp_page_pool = NULL;
    PagePool::PageListHead  tmp_page_list;
    Size                    msg_len = 0;
    Byte                    tmp_nal_length_size = 0;
    if (mt_unlikely (!muxAvcCodecData (&arr,
                                       avc_cdata.len(),
                                       &tmp_page_pool,
                                       &tmp_page_list,
                                       &msg_len,
                                       &tmp_nal_length_size)))
    {
        logE (tsmux, _this_func, "muxAvcCodecData() failed");
        return;
    }

    logD_ ("msg_len ", msg_len);
    PagePool::logPages (LogLevel::Debug, tmp_page_list.first, /*first_page_offs=*/ 0, _this_func);

    avc_cdata_page_pool = tmp_page_pool;
    avc_cdata_page_list = tmp_page_list;
    avc_cdata_len       = msg_len;
    nal_length_size     = tmp_nal_length_size;
}

void
TsMuxer::muxAvcAccessUnitData (Array * const mt_nonnull arr,
                               Size    const msg_len,
                               Uint64  const pts_nanosec,
                               Uint64  const dts_nanosec)
{
    if (mt_unlikely (!nal_length_size))
        return;

    // byte stream NAL unit start code
    Byte const startcode [3] = { 0x00, 0x00, 0x01 };
    // access unit delimiter
    Byte const au_delimiter [6] = { 0, 0, 0, 1, 0x09, 0xf0 };

    Size data_len  = 0;
    {
        Size in_offset = 0;
        bool is_idr = false;

        Byte nal_head [5];
        assert (nal_length_size <= 4 && nal_length_size != 3);
        Size const head_len = nal_length_size + 1;
        while (in_offset + head_len <= msg_len) {
            arr->get (in_offset, Memory (nal_head, head_len));

            Size nal_len = 0;
            switch (nal_length_size) {
                case 1: {
                    nal_len = nal_head [0];
                } break;
                case 2: {
                    nal_len = readBe16 (nal_head);
                } break;
                case 4: {
                    nal_len = readBe32 (nal_head);
                } break;
            }

            if (mt_unlikely (nal_len == 0)) {
                in_offset += nal_len + nal_length_size;
                continue;
            }

            if (mt_unlikely (in_offset + nal_len + nal_length_size > msg_len)) {
                logS (tsmux, _this_func,
                      "bad NAL unit length ", nal_len, " "
                      "at offset ", in_offset, ", "
                      "msg len ", msg_len);
                return;
            }

            Byte const nal_type = nal_head [nal_length_size] & 0x1f;

            if (   nal_type == 7 /* SPS */
                || nal_type == 8 /* PPS */
                || nal_type == 9 /* access unit delimiter */)
            {
                in_offset += nal_len + nal_length_size;
                continue;
            }

            if (nal_type == 5 /* coded slice of an IDR picture */)
                is_idr = true;

            in_offset += nal_len + nal_length_size;
            data_len  += nal_len + sizeof (startcode);
        }

        if (is_idr)
            data_len += avc_cdata_len;

        if (!data_len) {
            logD (tsmux, "no video data");
            return;
        }

        data_len += sizeof (au_delimiter);
    }

    Size const pes_header_size =
            (pts_nanosec == dts_nanosec ?
                       MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS
                     : MOMENT__TS_MUXER__PES_HEADER_SIZE__PTS_DTS);

    Size payload_bytes_available = pes_header_size + data_len;
    Byte *packet_buf = allocateTransportPacket ();

    Byte adaptation_field_control;
    Byte adaptation_field_flags;
    if (got_audio) {
        adaptation_field_control = MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD;
        adaptation_field_flags = 0;
    } else {
        adaptation_field_control =   MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD
                                   | MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__ADAPTATION_FIELD;
        adaptation_field_flags = MOMENT__TS_MUXER__PCR_FLAG;
    }

    // TODO Better integer scaling?
    Uint64 const pts = pts_nanosec * 9 / 100000;
    Uint64 const dts = dts_nanosec * 9 / 100000;

    Size packet_len = 0;
    packet_buf = fillTsHeader (packet_buf,
                               MOMENT__TS_MUXER__PAYLOAD_UNIT_START_INDICATOR,
                               MOMENT__TS_MUXER__VIDEO_PID,
                               &video_continuity_counter,
                               adaptation_field_control,
                               dts /* pcr */,
                               payload_bytes_available,
                               adaptation_field_flags,
                               &packet_len);

    {
        Size pes_header_len = 0;

        fillPesHeader (packet_buf,
                       MOMENT__TS_MUXER__STREAM_ID__AVC,
                       pts,
                       dts,
                       data_len,
                       &pes_header_len);
        assert (pes_header_len == pes_header_size);

        packet_buf += pes_header_len;
        packet_len -= pes_header_len;
        payload_bytes_available -= pes_header_len;
    }

    {
        TsFiller ts_filler (this /* ts_muxer */,
                            MOMENT__TS_MUXER__VIDEO_PID,
                            &video_continuity_counter,
                            payload_bytes_available,
                            packet_buf,
                            packet_len);

        ts_filler.addData (ConstMemory::forObject (au_delimiter));

        Size in_offset = 0;

        Byte nal_head [5];
        Size const head_len = nal_length_size + 1;
        bool cdata_added = false;
        while (in_offset + head_len <= msg_len) {
            arr->get (in_offset, Memory (nal_head, head_len));

            Size nal_len = 0;
            switch (nal_length_size) {
                case 1: {
                    nal_len = nal_head [0];
                } break;
                case 2: {
                    nal_len = readBe16 (nal_head);
                } break;
                case 4: {
                    nal_len = readBe32 (nal_head);
                } break;
            }

            if (mt_unlikely (nal_len == 0)) {
                in_offset += nal_len + nal_length_size;
                continue;
            }

            Byte const nal_type = nal_head [nal_length_size] & 0x1f;

            if (   nal_type == 7 /* SPS */
                || nal_type == 8 /* PPS */
                || nal_type == 9 /* access unit delimiter */)
            {
                in_offset += nal_len + nal_length_size;
                continue;
            }

            if (nal_type == 5 /* coded slice of an IDR picture */
                && !cdata_added)
            {
                cdata_added = true;

                PagePool::PageListArray avc_cdata_arr (avc_cdata_page_list.first,
                                                       /*offset=*/ 0,
                                                       avc_cdata_len);
                ts_filler.addDataFromArray (&avc_cdata_arr, 0 /* offset */, avc_cdata_len);
            }

            ts_filler.addData (ConstMemory::forObject (startcode));
            ts_filler.addDataFromArray (arr, in_offset + nal_length_size, nal_len);

            in_offset += nal_len + nal_length_size;
        }
    }
}

void
TsMuxer::muxAvcMessage (VideoMessage * const mt_nonnull video_msg)
{
    PagePool::PageListArray arr (video_msg->page_list.first,
                                 video_msg->msg_offset,
                                 video_msg->msg_len);

    if (mt_unlikely (video_msg->frame_type == VideoFrameType::AvcSequenceHeader)) {
        if (avc_cdata_page_pool) {
            avc_cdata_page_pool->msgUnref (avc_cdata_page_list.first);
            avc_cdata_page_list.reset ();
            avc_cdata_page_pool = NULL;
        }

        PagePool               *tmp_page_pool = NULL;
        PagePool::PageListHead  tmp_page_list;
        Size                    msg_len = 0;
        Byte                    tmp_nal_length_size = 0;
        if (mt_unlikely (!muxAvcCodecData (&arr,
                                           video_msg->msg_len,
                                           &tmp_page_pool,
                                           &tmp_page_list,
                                           &msg_len,
                                           &tmp_nal_length_size)))
        {
            logE (tsmux, _this_func, "muxAvcCodecData() failed");
            return;
        }

        avc_cdata_page_pool = tmp_page_pool;
        avc_cdata_page_list = tmp_page_list;
        avc_cdata_len       = msg_len;
        nal_length_size     = tmp_nal_length_size;
        return;
    }

    if (mt_unlikely (!video_msg->frame_type.isVideoData()))
        return;

    muxAvcAccessUnitData (&arr,
                          video_msg->msg_len,
                          video_msg->pts_nanosec,
                          video_msg->dts_nanosec);
}

void
TsMuxer::writePAT ()
{
    Byte *buf = allocateTransportPacket ();

    Size payload_len;
    buf = fillTsHeader (buf,
                        MOMENT__TS_MUXER__PAYLOAD_UNIT_START_INDICATOR,
                        0 /* pid */,
                        &pat_continuity_counter,
                        MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD,
                        0 /* pcr */,
                        // Max payload size to avoid adding
                        // an adaptation field with stuffing bytes.
                        MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE - 4 /* payload_bytes_available */,
                        0 /* adaptation_field_flags */,
                        &payload_len);
    assert (payload_len >= 17);

    // pointer_field
    buf [0] = 0;
    ++buf;
    --payload_len;

    // 8 bits: table_id
    buf [0] = 0;

    //  1 bit : section_syntax_indicator = 1
    //  1 bit : 0
    //  2 bits: reserved = 0x3
    // 12 bits: section_length
    writeBe16 (buf + 1, 0xb000 | 13);

    // 16 bits: transport_stream_id
    writeBe16 (buf + 3, 1);

    // 2 bits: reserved
    // 5 bits: version_number
    // 1 bit : curent_next_indicator
    buf [5] = 0xc1;

    // 8 bits: section_number
    buf [6] = 0;
    // 8 bits: last_section_number
    buf [7] = 0;

    // 16 bits: program_number
    writeBe16 (buf +  8, MOMENT__TS_MUXER__PROGRAM_NUMBER);
    //  3 bits: reserved
    // 13 bits: program_map_PID
    writeBe16 (buf + 10, 0xe000 | MOMENT__TS_MUXER__PMT_PID);

    // 32 bits: CRC_32
    writeBe32 (buf + 12, crc32 (buf, 12));

    buf += 16;
    payload_len -= 16;

    // stuffing bytes
    memset (buf, 0xff, payload_len);
}

void
TsMuxer::writePMT ()
{
    Byte *buf = allocateTransportPacket ();

    Size payload_len;
    buf = fillTsHeader (buf,
                        MOMENT__TS_MUXER__PAYLOAD_UNIT_START_INDICATOR,
                        MOMENT__TS_MUXER__PMT_PID,
                        &pmt_continuity_counter,
                        MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD,
                        0 /* pcr */,
                        // Max payload size to avoid adding
                        // an adaptation field with stuffing bytes.
                        MOMENT__TS_MUXER__TRANSPORT_PACKET_SIZE - 4 /* payload_bytes_available */,
                        0 /* adaptation_field_flags */,
                        &payload_len);

    // pointer_field
    buf [0] = 0;
    ++buf;
    --payload_len;
    Byte * const crc_start = buf;

    // 8 bits: table_id
    buf [0] = 0x02;

    //  1 bit : section_syntax_indicator = 1
    //  1 bit : 0
    //  2 bits: reserved = 0x3
    // 12 bits: section_length
    // (filled in below)
    Byte * const len_ptr = buf + 1;

    // 16 bits: program_number
    writeBe16 (buf + 3, MOMENT__TS_MUXER__PROGRAM_NUMBER);

    // 2 bits: reserved
    // 5 bits: version_number
    // 1 bit : current_next_indicator
    buf [5] = 0xc1;
    // 8 bits: section_number
    buf [6] = 0;
    // 9 bits: last_section_number
    buf [7] = 0;

    //  3 bits: reserved
    // 13 bits: PCR_PID
    {
        Uint16 pcr_pid;
        if (got_audio)
            pcr_pid = MOMENT__TS_MUXER__AUDIO_PID;
        else
        if (got_video)
            pcr_pid = MOMENT__TS_MUXER__VIDEO_PID;
        else
            pcr_pid = 0x1fff;

        writeBe16 (buf + 8, 0xe000 | pcr_pid);
    }

    //  4 bits: reserved
    // 12 bits: program_info_length
    writeBe16 (buf + 10, 0xf000 | 0);
    // descriptor (0 bits)

    buf += 12;
    payload_len -= 12;

    if (got_audio) {
        //  8 bits: stream_type (AAC)
        buf [0] = MOMENT__TS_MUXER__STREAM_TYPE__AAC;
        //  3 bits: reserved
        // 13 bits: elementary_PID
        writeBe16 (buf + 1, 0xe000 | MOMENT__TS_MUXER__AUDIO_PID);

        //  4 bits: reserved
        // 12 bits: ES_info_length
        writeBe16 (buf + 3, 0xf000 | 0);
        // descriptor (0 bits)

        buf += 5;
        payload_len -= 5;
    }

    if (got_video) {
        //  8 bits: stream_type (AVC)
        buf [0] = MOMENT__TS_MUXER__STREAM_TYPE__AVC;
        //  3 bits: reserved
        // 13 bits: elementary_PID
        writeBe16 (buf + 1, 0xe000 | MOMENT__TS_MUXER__VIDEO_PID);

        //  4 bits: reserved
        // 12 bits: ES_info_length
        writeBe16 (buf + 3, 0xf000 | 0);
        // descriptor (0 bits)

        buf += 5;
        payload_len -= 5;
    }

    buf += 4;
    payload_len -= 4;

    writeBe16 (len_ptr, 0xb000 | (buf - (len_ptr + 2)));

    // 32 bits: CRC_32
    writeBe32 (buf - 4, crc32 (crc_start, buf - 4 - crc_start));

    // stuffing bytes
    memset (buf, 0xff, payload_len);
}

void
TsMuxer::stuffContinuityCounters ()
{
    // PAT/PMT continuity counter stuffing is unnecessary.
    //
    // assert ((pat_continuity_counter & 0x0f) == 0);
    // assert ((pmt_continuity_counter & 0x0f) == 0);

    while ((audio_continuity_counter & 0x0f) != 0) {
      // It turns out that stuffing is not necessary for audio, probably
      // as long as separate PES packets are used for every audio frame
      // (or is that not an issue as well?)
        break;
    }

    while ((video_continuity_counter & 0x0f) != 0) {
        Byte filler_data_nal [6] = { 0, 0, 1, 0x0c, 0xff, 0x80 };

        Byte *packet_buf = allocateTransportPacket ();

        Size packet_len = 0;
        packet_buf = fillTsHeader (packet_buf,
                                   0,
                                   MOMENT__TS_MUXER__VIDEO_PID,
                                   &video_continuity_counter,
                                   MOMENT__TS_MUXER__ADAPTATION_FIELD_CONTROL__PAYLOAD,
                                   0 /* pcr */,
                                   sizeof (filler_data_nal),
                                   0 /* adaptation_field_flags */,
                                   &packet_len);
        assert (packet_len == sizeof (filler_data_nal));

        memcpy (packet_buf, filler_data_nal, sizeof (filler_data_nal));
    }
}

void
TsMuxer::writeProgramTables ()
{
    // PAT/PMT continuity counter stuffing is unnecessary.
    //
    // If it was necessary, then we could repeating the frames 16 times to set
    // continuity counters to 0x0f, or we could initialize PAT/PMT continuity
    // counter with HLS segment indexes (better).
    //
    for (unsigned i = 0; i < 1 /* 16 */; ++i) {
        writePAT ();
        writePMT ();
    }
}

PagePool::PageListHead
TsMuxer::flush (Size * const mt_nonnull ret_len)
{
    *ret_len = total_data_len;
    total_data_len = 0;

    PagePool::PageListHead tmp_page_list = page_list;
    page_list.reset ();

    return tmp_page_list;
}

mt_const void
TsMuxer::init (PagePool * const mt_nonnull page_pool,
               bool       const got_audio,
               bool       const got_video)
{
    this->page_pool = page_pool;
    this->got_audio = got_audio;
    this->got_video = got_video;
}

TsMuxer::TsMuxer ()
    : page_pool                (NULL),
      got_audio                (false),
      got_video                (false),
      pat_continuity_counter   (0),
      pmt_continuity_counter   (0),
      audio_continuity_counter (0),
      video_continuity_counter (0),
      aac_cdata_page_pool      (NULL),
      aac_cdata_offset         (0),
      aac_cdata_len            (0),
      avc_cdata_page_pool      (NULL),
      avc_cdata_len            (0),
      nal_length_size          (0),
      total_data_len           (0)
{
}

TsMuxer::~TsMuxer ()
{
    if (page_pool)
        page_pool->msgUnref (page_list.first);

    if (aac_cdata_page_pool)
        aac_cdata_page_pool->msgUnref (aac_cdata_page_list.first);

    if (avc_cdata_page_pool)
        avc_cdata_page_pool->msgUnref (avc_cdata_page_list.first);
}

}

