/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/nal_reader.h>

#include <moment/mp4_muxer.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_mp4mux   ("moment.mp4mux", LogLevel::I);

void
Mp4Muxer::patchTrackStco (TrackInfo * const mt_nonnull track,
                          Uint32      const offset)
{
    PagePool::PageListArray stco_arr (track->stco_pages.first, 0 /* offset*/, track->stco_pos /* data_len */);
    Byte stco_entry [4];
    for (Size i = 0; i < track->stco_pos; i += 4) {
        stco_arr.get (i, Memory::forObject (stco_entry));

        Uint32 const value = offset + (((Uint32) stco_entry [0] << 24) |
                                       ((Uint32) stco_entry [1] << 16) |
                                       ((Uint32) stco_entry [2] <<  8) |
                                       ((Uint32) stco_entry [3] <<  0));
        stco_entry [0] = (Byte) (value >> 24);
        stco_entry [1] = (Byte) (value >> 16);
        stco_entry [2] = (Byte) (value >>  8);
        stco_entry [3] = (Byte) (value >>  0);
        stco_arr.set (i, ConstMemory::forObject (stco_entry));
    }
}

PagePool::PageListInfo
Mp4Muxer::writeMoovAtom (Time const duration_millisec,
                         bool const moov_only,
                         bool const moov_tail)
{
  // TODO Сжатие таблиц в stbl

    bool const got_audio = audio_track.num_frames > 0;
    bool const got_video = video_track.num_frames > 0;

    // mac time - 2082844800 = unixtime
    Uint32 const mactime = (Uint32) (getUnixtime() + 2082844800);

    Byte ftyp_data [] = {
        0x00, 0x00, 0x00, 0x20,
         'f',  't',  'y',  'p',
         'm',  'p',  '4',  '2',
        0x00, 0x00, 0x00, 0x00,
         'm',  'p',  '4',  '2',
         'm',  'p',  '4',  '1',
         'i',  's',  'o',  'm',
         'i',  's',  'o',  '2'
      /* For QT:
         'q',  't',  ' ',  ' ',
        0x20, 0x04, 0x06, 0x00,
         'q',  't',  ' ',  ' '
       */
    };

    Byte moov_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'o',  'o',  'v',
        0x00, 0x00, 0x00, 0x6c,
         'm',  'v',  'h',  'd',
        // 1 byte version, 3 bytes flags
        0x00, 0x00, 0x00, 0x00,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // timescale (1000)
        0x00, 0x00, 0x03, 0xe8,
        // duration
        (Byte) (duration_millisec >> 24),
        (Byte) (duration_millisec >> 16),
        (Byte) (duration_millisec >>  8),
        (Byte) (duration_millisec >>  0),
        // rate (1.0)
        0x00, 0x01, 0x00, 0x00,
        // volume (1.0)
        0x01, 0x00,
        // reserved
                    0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // matrix
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00,
        // preview time
        0x00, 0x00, 0x00, 0x00,
        // preview duration
        0x00, 0x00, 0x00, 0x00,
        // poster time
        0x00, 0x00, 0x00, 0x00,
        // selection time
        0x00, 0x00, 0x00, 0x00,
        // selection duration
        0x00, 0x00, 0x00, 0x00,
        // current time
        0x00, 0x00, 0x00, 0x00,
        // next track id
        0x00, 0x00, 0x00, (Byte) (1 + (got_audio ? 1 : 0) + (got_video ? 1 : 0))
    };

    Byte audio_trak_data [] = {
        0x00, 0x00, 0x00, 0x00,
         't',  'r',  'a',  'k',
    };

    Byte audio_tkhd_data [] = {
        0x00, 0x00, 0x00, 0x5c,
         't',  'k',  'h',  'd',
        // version and flags
        0x00, 0x00, 0x00, 0x07,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // track id
        0x00, 0x00, 0x00, 0x01,
        // reserved
        0x00, 0x00, 0x00, 0x00,
        // duration
        (Byte) (duration_millisec >> 24),
        (Byte) (duration_millisec >> 16),
        (Byte) (duration_millisec >>  8),
        (Byte) (duration_millisec >>  0),
        // reserved
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // layer, alternate group
        0x00, 0x00, 0x00, 0x00,
        // volume, reserved
        0x01, 0x00, 0x00, 0x00,
        // matrix
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00,
        // track width
        0x00, 0x00, 0x00, 0x00,
        // track height
        0x00, 0x00, 0x00, 0x00,
    };

    Byte audio_mdia_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'd',  'i',  'a',
        0x00, 0x00, 0x00, 0x20,
         'm',  'd',  'h',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
//#warning TODO time scale = sample rate?
        // time scale (3000)
        0x00, 0x00, 0x0b, 0xb8,
        // duration
        (Byte) ((duration_millisec * 3) >> 24),
        (Byte) ((duration_millisec * 3) >> 16),
        (Byte) ((duration_millisec * 3) >>  8),
        (Byte) ((duration_millisec * 3) >>  0),
        // language, quality
        0x00, 0x00, 0x00, 0x00
    };

    Byte audio_mdia_hdlr_data [] = {
        0x00, 0x00, 0x00, 0x2d,
         'h',  'd',  'l',  'r',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // component type
        // For QT: 'm',  'h',  'l',  'r',
        0x00, 0x00, 0x00, 0x00,
        // component subtype
        's',  'o',  'u',  'n',
        // component manufacturer
        0x00, 0x00, 0x00, 0x00,
        // component flags
        0x00, 0x00, 0x00, 0x00,
        // component flags mask
        0x00, 0x00, 0x00, 0x00,
        // component name
        0x0c,
        'S','o','u','n',
        'd','H','a','n',
        'd','l','e','r'
    };

    Byte audio_minf_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'i',  'n',  'f',

        0x00, 0x00, 0x00, 0x10,
         's',  'm',  'h',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // balance, reserved
        0x00, 0x00, 0x00, 0x00,

        0x00, 0x00, 0x00, 0x24,
         'd',  'i',  'n',  'f',

        0x00, 0x00, 0x00, 0x1c,
         'd',  'r',  'e',  'f',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,

        0x00, 0x00, 0x00, 0x0c,
         'u',  'r',  'l',  ' ',
        // For QT:  'a',  'l',  'i',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x01,
    };

    Size const audio_esds_size = audio_track.hdr_size ? (0x24 + 1 + audio_track.hdr_size) : 0;
    Size const audio_mp4a_size = 0x24 + audio_esds_size;
    Size const audio_stsd_size = 16 + audio_mp4a_size;

    Uint32 const audio_channels = (media_desc__audio ? media_desc__audio->channels : 2);
    Uint32 const audio_rate     = (media_desc__audio ? media_desc__audio->rate : 44100);

    Byte audio_stbl_data [] = {
        0x00, 0x00, 0x00, 0x00,
         's',  't',  'b',  'l',

        (Byte) (audio_stsd_size >> 24),
        (Byte) (audio_stsd_size >> 16),
        (Byte) (audio_stsd_size >>  8),
        (Byte) (audio_stsd_size >>  0),
         's',  't',  's',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,

        (Byte) (audio_mp4a_size >> 24),
        (Byte) (audio_mp4a_size >> 16),
        (Byte) (audio_mp4a_size >>  8),
        (Byte) (audio_mp4a_size >>  0),
         'm',  'p',  '4',  'a',
        // reserved
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // data reference index
        0x00, 0x01,
        // version, revision
        0x00, 0x00, 0x00, 0x00,
        // vendor
        0x00, 0x00, 0x00, 0x00,

        // number of channels, sample size (16 bit)
        0x00, (Byte) audio_channels, 0x00, 0x10,
        // compression id, packet size
        0x00, 0x00, 0x00, 0x00,
        // sample rate
        (Byte) ((audio_rate >> 8) & 0xff), (Byte) ((audio_rate >> 0) & 0xff), 0x00, 0x00,
    };

    Byte audio_esds_head_data [] = {
        (Byte) (audio_esds_size >> 24),
        (Byte) (audio_esds_size >> 16),
        (Byte) (audio_esds_size >>  8),
        (Byte) (audio_esds_size >>  0),
        // For QT: 'wave' atom
         'e',  's',  'd',  's',
        // version
        0x00, 0x00, 0x00, 0x00,
        // ES_Descriptor
//#warning 0x19 is descriptor size in bytes. It's valid only when AAC cdata is 2 bytes long.
        0x03, 0x19, 0x00, 0x01,
//#warning 0x11 is also descriptor size (DecoderConfigDescriptor)
        0x00, 0x04, 0x11, 0x40,
        0x15, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xa7, 0x25, // avg bitrate
        // DecoderSpecificInfo (short)
        0x05,
        // length
        0x00
    };

    Byte audio_esds_tail_data [] = {
//#warning Why is this necessary?
        // SLConfigDescriptor
        0x06, 0x01, 0x02
    };

    // sample-to-chunk atom
    Byte audio_stsc_data [] = {
        0x00, 0x00, 0x00, 0x1c,
         's',  't',  's',  'c',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,
        // first chunk
        0x00, 0x00, 0x00, 0x01,
        // samples per chunk
        0x00, 0x00, 0x00, 0x01,
#if 0
// "audio only" case
        (Byte) (audio_track.num_frames >> 24),
        (Byte) (audio_track.num_frames >> 16),
        (Byte) (audio_track.num_frames >>  8),
        (Byte) (audio_track.num_frames >>  0),
#endif
        // sample description id
        0x00, 0x00, 0x00, 0x01
    };

#if 0
// "audio only" case
    // chunk offset atom
    Byte audio_stco_data [] = {
        0x00, 0x00, 0x00, 0x14,
         's',  't',  'c',  'o',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,
        // offset (filled below)
        0x00, 0x00, 0x00, 0x00
    };
#endif

    Size const audio_stco_size = 16 + audio_track.num_frames * 4;
    Byte audio_stco_data [] = {
        (Byte) (audio_stco_size >> 24),
        (Byte) (audio_stco_size >> 16),
        (Byte) (audio_stco_size >>  8),
        (Byte) (audio_stco_size >>  0),
         's',  't',  'c',  'o',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (audio_track.num_frames >> 24),
        (Byte) (audio_track.num_frames >> 16),
        (Byte) (audio_track.num_frames >>  8),
        (Byte) (audio_track.num_frames >>  0)
    };

    Size const audio_stts_size = 16 + audio_track.num_frames * 8;
    // time-to-sample atom
    Byte audio_stts_data [] = {
        (Byte) (audio_stts_size >> 24),
        (Byte) (audio_stts_size >> 16),
        (Byte) (audio_stts_size >>  8),
        (Byte) (audio_stts_size >>  0),
         's',  't',  't',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (audio_track.num_frames >> 24),
        (Byte) (audio_track.num_frames >> 16),
        (Byte) (audio_track.num_frames >>  8),
        (Byte) (audio_track.num_frames >>  0)
    };

    Size const audio_stsz_size = 20 + audio_track.num_frames * 4;
    // sample size atom
    Byte audio_stsz_data [] = {
        (Byte) (audio_stsz_size >> 24),
        (Byte) (audio_stsz_size >> 16),
        (Byte) (audio_stsz_size >>  8),
        (Byte) (audio_stsz_size >>  0),
         's',  't',  's',  'z',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // sample size
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (audio_track.num_frames >> 24),
        (Byte) (audio_track.num_frames >> 16),
        (Byte) (audio_track.num_frames >>  8),
        (Byte) (audio_track.num_frames >>  0)
    };

    Size const audio_ctts_size = 16 + audio_track.num_frames * 8;
    // composition offset atom
    Byte const audio_ctts_data [] = {
        (Byte) (audio_ctts_size >> 24),
        (Byte) (audio_ctts_size >> 16),
        (Byte) (audio_ctts_size >>  8),
        (Byte) (audio_ctts_size >>  0),
         'c',  't',  't',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (audio_track.num_frames >> 24),
        (Byte) (audio_track.num_frames >> 16),
        (Byte) (audio_track.num_frames >>  8),
        (Byte) (audio_track.num_frames >>  0)
    };

    Byte video_trak_data [] = {
        0x00, 0x00, 0x00, 0x00,
         't',  'r',  'a',  'k',
    };

    Uint32 const video_width  = media_desc__video ? media_desc__video->width  : avc_hdr_width;
    Uint32 const video_height = media_desc__video ? media_desc__video->height : avc_hdr_height;

    Byte video_tkhd_data [] = {
        0x00, 0x00, 0x00, 0x5c,
         't',  'k',  'h',  'd',
        // version and flags
        0x00, 0x00, 0x00, 0x07,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // track id
        0x00, 0x00, 0x00, (got_audio ? (Byte) 0x02 : (Byte) 0x01),
        // reserved
        0x00, 0x00, 0x00, 0x00,
        // duration
        (Byte) (duration_millisec >> 24),
        (Byte) (duration_millisec >> 16),
        (Byte) (duration_millisec >>  8),
        (Byte) (duration_millisec >>  0),
        // reserved
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // layer, alternate group
        0x00, 0x00, 0x00, 0x00,
        // volume, reserved
        0x00, 0x00, 0x00, 0x00,
        // matrix
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00,
        // track width
        (Byte) ((video_width  >> 8) & 0xff), (Byte) ((video_width  >> 0) & 0xff), 0x00, 0x00,
        // track height
        (Byte) ((video_height >> 8) & 0xff), (Byte) ((video_height >> 0) & 0xff), 0x00, 0x00,
    };

    Byte video_mdia_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'd',  'i',  'a',
        0x00, 0x00, 0x00, 0x20,
         'm',  'd',  'h',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // creation time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // modification time
        (Byte) (mactime >> 24),
        (Byte) (mactime >> 16),
        (Byte) (mactime >>  8),
        (Byte) (mactime >>  0),
        // time scale (3000)
        0x00, 0x00, 0x0b, 0xb8,
        // duration
        (Byte) ((duration_millisec * 3) >> 24),
        (Byte) ((duration_millisec * 3) >> 16),
        (Byte) ((duration_millisec * 3) >>  8),
        (Byte) ((duration_millisec * 3) >>  0),
        // language, quality
        0x00, 0x00, 0x00, 0x00
    };

    Byte video_mdia_hdlr_data [] = {
        0x00, 0x00, 0x00, 0x2d,
         'h',  'd',  'l',  'r',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // component type
        // For QT: 'm',  'h',  'l',  'r',
        0x00, 0x00, 0x00, 0x00,
        // component subtype
        'v',  'i',  'd',  'e',
        // component manufacturer
        0x00, 0x00, 0x00, 0x00,
        // component flags
        0x00, 0x00, 0x00, 0x00,
        // component flags mask
        0x00, 0x00, 0x00, 0x00,
        // component name
        0x0c,
        'V','i','d','e',
        'o','H','a','n',
        'd','l','e','r'
    };

    Byte video_minf_data [] = {
        0x00, 0x00, 0x00, 0x00,
         'm',  'i',  'n',  'f',

        0x00, 0x00, 0x00, 0x14,
         'v',  'm',  'h',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
      /* For QT
        // graphics mode: dither copy
        0x00, 0x40,
        // opcolor
        0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
       */

      /* For QT
        0x00, 0x00, 0x00, 0x21,
         'h',  'd',  'l',  'r',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // component type
         'd',  'h',  'l',  'r',
        // component subtype
         'a',  'l',  'i',  's',
        // component manufacturer
        0x00, 0x00, 0x00, 0x00,
        // component flags
        0x00, 0x00, 0x00, 0x00,
        // component flags mask
        0x00, 0x00, 0x00, 0x00,
        // component name
        0x00,
       */

        0x00, 0x00, 0x00, 0x24,
         'd',  'i',  'n',  'f',

        0x00, 0x00, 0x00, 0x1c,
         'd',  'r',  'e',  'f',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,

        0x00, 0x00, 0x00, 0x0c,
         'u',  'r',  'l',  ' ',
        // For QT: 'a',  'l',  'i',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x01,
    };

    Size const video_avcc_size = video_track.hdr_size ? (8 + video_track.hdr_size) : 0;
    Size const video_avc1_size = 0x56 + video_avcc_size;
    Size const video_stsd_size = 16 + video_avc1_size;
    Byte video_stbl_data [] = {
        0x00, 0x00, 0x00, 0x00,
         's',  't',  'b',  'l',

        (Byte) (video_stsd_size >> 24),
        (Byte) (video_stsd_size >> 16),
        (Byte) (video_stsd_size >>  8),
        (Byte) (video_stsd_size >>  0),
         's',  't',  's',  'd',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,

        (Byte) (video_avc1_size >> 24),
        (Byte) (video_avc1_size >> 16),
        (Byte) (video_avc1_size >>  8),
        (Byte) (video_avc1_size >>  0),
         'a',  'v',  'c',  '1',
        // reserved
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // data reference index
        0x00, 0x01,
        // version, revision
        0x00, 0x00, 0x00, 0x00,
        // vendor
        0x00, 0x00, 0x00, 0x00,
        // temporal quality
        0x00, 0x00, 0x00, 0x00,
        // For QT: 0x00, 0x00, 0x02, 0x00,
        // spatial quality
        0x00, 0x00, 0x00, 0x00,
        // For QT: 0x00, 0x00, 0x02, 0x00,
        // width, height
        (Byte) ((video_width  >> 8) & 0xff), (Byte) ((video_width  >> 0) & 0xff),
        (Byte) ((video_height >> 8) & 0xff), (Byte) ((video_height >> 0) & 0xff),
        // horizontal resolution (72 dpi)
        0x00, 0x48, 0x00, 0x00,
        // vertical resolution (72 dpi)
        0x00, 0x48, 0x00, 0x00,
        // data size
        0x00, 0x00, 0x00, 0x00,
        // frame count
        0x00, 0x01,
        // compressor name
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        // depth (24 bit)
        0x00, 0x18,
        // color table id
        0xff, 0xff,
    };

    Byte video_avcc_data [] = {
        (Byte) (video_avcc_size >> 24),
        (Byte) (video_avcc_size >> 16),
        (Byte) (video_avcc_size >>  8),
        (Byte) (video_avcc_size >>  0),
         'a',  'v',  'c',  'C'
    };

    // sample-to-chunk atom
    Byte video_stsc_data [] = {
        0x00, 0x00, 0x00, 0x1c,
         's',  't',  's',  'c',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,
        // first chunk
        0x00, 0x00, 0x00, 0x01,
        // samples per chunk
        0x00, 0x00, 0x00, 0x01,
#if 0
// "video only" case
        (Byte) (video_track.num_frames >> 24),
        (Byte) (video_track.num_frames >> 16),
        (Byte) (video_track.num_frames >>  8),
        (Byte) (video_track.num_frames >>  0),
#endif
        // sample description id
        0x00, 0x00, 0x00, 0x01
    };

#if 0
// "video only" case
    // chunk offset atom
    Byte video_stco_data [] = {
        0x00, 0x00, 0x00, 0x14,
         's',  't',  'c',  'o',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        0x00, 0x00, 0x00, 0x01,
        // offset (filled below)
        0x00, 0x00, 0x00, 0x00
    };
#endif

    Size const video_stco_size = 16 + video_track.num_frames * 4;
    Byte video_stco_data [] = {
        (Byte) (video_stco_size >> 24),
        (Byte) (video_stco_size >> 16),
        (Byte) (video_stco_size >>  8),
        (Byte) (video_stco_size >>  0),
         's',  't',  'c',  'o',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (video_track.num_frames >> 24),
        (Byte) (video_track.num_frames >> 16),
        (Byte) (video_track.num_frames >>  8),
        (Byte) (video_track.num_frames >>  0)
    };

    Size const video_stts_size = 16 + video_track.num_frames * 8;
    // time-to-sample atom
    Byte video_stts_data [] = {
        (Byte) (video_stts_size >> 24),
        (Byte) (video_stts_size >> 16),
        (Byte) (video_stts_size >>  8),
        (Byte) (video_stts_size >>  0),
         's',  't',  't',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (video_track.num_frames >> 24),
        (Byte) (video_track.num_frames >> 16),
        (Byte) (video_track.num_frames >>  8),
        (Byte) (video_track.num_frames >>  0)
    };

    Size const video_stss_size = 16 + video_track.num_stss_entries * 4;
    // sync sample atom
    Byte video_stss_data [] = {
        (Byte) (video_stss_size >> 24),
        (Byte) (video_stss_size >> 16),
        (Byte) (video_stss_size >>  8),
        (Byte) (video_stss_size >>  0),
         's',  't',  's',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (video_track.num_stss_entries >> 24),
        (Byte) (video_track.num_stss_entries >> 16),
        (Byte) (video_track.num_stss_entries >>  8),
        (Byte) (video_track.num_stss_entries >>  0)
    };

    Size const video_stsz_size = 20 + video_track.num_frames * 4;
    // sample size atom
    Byte video_stsz_data [] = {
        (Byte) (video_stsz_size >> 24),
        (Byte) (video_stsz_size >> 16),
        (Byte) (video_stsz_size >>  8),
        (Byte) (video_stsz_size >>  0),
         's',  't',  's',  'z',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // sample size
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (video_track.num_frames >> 24),
        (Byte) (video_track.num_frames >> 16),
        (Byte) (video_track.num_frames >>  8),
        (Byte) (video_track.num_frames >>  0)
    };

    Size const video_ctts_size = 16 + video_track.num_frames * 8;
    // composition offset atom
    Byte const video_ctts_data [] = {
        (Byte) (video_ctts_size >> 24),
        (Byte) (video_ctts_size >> 16),
        (Byte) (video_ctts_size >>  8),
        (Byte) (video_ctts_size >>  0),
         'c',  't',  't',  's',
        // version, flags
        0x00, 0x00, 0x00, 0x00,
        // number of entries
        (Byte) (video_track.num_frames >> 24),
        (Byte) (video_track.num_frames >> 16),
        (Byte) (video_track.num_frames >>  8),
        (Byte) (video_track.num_frames >>  0)
    };

    Size const mdat_size = 8 + audio_track.total_frame_size + video_track.total_frame_size;
    Byte const mdat_data [] = {
        (Byte) (mdat_size >> 24),
        (Byte) (mdat_size >> 16),
        (Byte) (mdat_size >>  8),
        (Byte) (mdat_size >>  0),
         'm',  'd',  'a',  't'
    };

    Size const audio_stbl_size = 8 + audio_stsd_size + sizeof (audio_stsc_data) +
                                 audio_stco_size + audio_stts_size + audio_stsz_size + audio_ctts_size;

    Size const audio_minf_size = 0x10 /* smhd */ + 0x24 /* dinf */ + audio_stbl_size + 8 /* minf header */;
    Size const audio_mdia_size = 0x20 /* mdhd */ + 0x2d /* mdia_hdlr */ + audio_minf_size + 8 /* mdia header */;
    Size const audio_trak_size = 0x5c /* tkhd */ + audio_mdia_size + 8 /* trak header */;

    Size const video_stbl_size = 8 + video_stsd_size + sizeof (video_stsc_data) +
                                 video_stco_size + video_stts_size + video_stss_size + video_stsz_size + video_ctts_size;

    Size const video_minf_size = 0x14 /* vmhd */ /* + For QT 0x21 hdlr */ + 0x24 /* dinf */ + video_stbl_size + 8 /* minf header */;
    Size const video_mdia_size = 0x20 /* mdhd */ + 0x2d /* mdia_hdlr */ + video_minf_size + 8 /* mdia header */;
    Size const video_trak_size = 0x5c /* tkhd */ + video_mdia_size + 8 /* trak header */;

    Size const moov_size = sizeof (moov_data) + (got_video ? video_trak_size : 0) + (got_audio ? audio_trak_size : 0);

    audio_stbl_data [0] = (Byte) (audio_stbl_size >> 24);
    audio_stbl_data [1] = (Byte) (audio_stbl_size >> 16);
    audio_stbl_data [2] = (Byte) (audio_stbl_size >>  8);
    audio_stbl_data [3] = (Byte) (audio_stbl_size >>  0);

    audio_minf_data [0] = (Byte) (audio_minf_size >> 24);
    audio_minf_data [1] = (Byte) (audio_minf_size >> 16);
    audio_minf_data [2] = (Byte) (audio_minf_size >>  8);
    audio_minf_data [3] = (Byte) (audio_minf_size >>  0);

    audio_mdia_data [0] = (Byte) (audio_mdia_size >> 24);
    audio_mdia_data [1] = (Byte) (audio_mdia_size >> 16);
    audio_mdia_data [2] = (Byte) (audio_mdia_size >>  8);
    audio_mdia_data [3] = (Byte) (audio_mdia_size >>  0);

    audio_trak_data [0] = (Byte) (audio_trak_size >> 24);
    audio_trak_data [1] = (Byte) (audio_trak_size >> 16);
    audio_trak_data [2] = (Byte) (audio_trak_size >>  8);
    audio_trak_data [3] = (Byte) (audio_trak_size >>  0);

    video_stbl_data [0] = (Byte) (video_stbl_size >> 24);
    video_stbl_data [1] = (Byte) (video_stbl_size >> 16);
    video_stbl_data [2] = (Byte) (video_stbl_size >>  8);
    video_stbl_data [3] = (Byte) (video_stbl_size >>  0);

    video_minf_data [0] = (Byte) (video_minf_size >> 24);
    video_minf_data [1] = (Byte) (video_minf_size >> 16);
    video_minf_data [2] = (Byte) (video_minf_size >>  8);
    video_minf_data [3] = (Byte) (video_minf_size >>  0);

    video_mdia_data [0] = (Byte) (video_mdia_size >> 24);
    video_mdia_data [1] = (Byte) (video_mdia_size >> 16);
    video_mdia_data [2] = (Byte) (video_mdia_size >>  8);
    video_mdia_data [3] = (Byte) (video_mdia_size >>  0);

    video_trak_data [0] = (Byte) (video_trak_size >> 24);
    video_trak_data [1] = (Byte) (video_trak_size >> 16);
    video_trak_data [2] = (Byte) (video_trak_size >>  8);
    video_trak_data [3] = (Byte) (video_trak_size >>  0);

    moov_data [0] = (Byte) (moov_size >> 24);
    moov_data [1] = (Byte) (moov_size >> 16);
    moov_data [2] = (Byte) (moov_size >>  8);
    moov_data [3] = (Byte) (moov_size >>  0);

    Size const stco_offset = sizeof (ftyp_data) + 8 /* mdat header */ + (moov_tail ? 0 : moov_size);
    patchTrackStco (&audio_track, stco_offset);
    patchTrackStco (&video_track, stco_offset);

    PagePool::PageListInfo pages;

    if (!moov_only) {
        page_pool->getFillPages (&pages, ConstMemory::forObject (ftyp_data));
    }
    page_pool->getFillPages (&pages, ConstMemory::forObject (moov_data));

    if (got_audio) {
        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_trak_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_tkhd_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_mdia_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_mdia_hdlr_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_minf_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_stbl_data));

        if (audio_track.hdr_size) {
            if (audio_track.hdr_size < 256) {
                audio_esds_head_data [sizeof (audio_esds_head_data) - 1] = (Byte) audio_track.hdr_size;
                page_pool->getFillPages (&pages, ConstMemory::forObject (audio_esds_head_data));
                page_pool->getFillPagesFromPages (&pages,
                                                  audio_track.hdr_msg,
                                                  audio_track.hdr_offs,
                                                  audio_track.hdr_size);
                audio_track.hdr_msg = NULL;
                page_pool->getFillPages (&pages, ConstMemory::forObject (audio_esds_tail_data));
            } else {
                logE_ (_func, "WARNING: UNSUPPORTED: AAC sequence header is larger than 255 bytes");
            }
        }

        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_stsc_data));

        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_stco_data));
        pages.appendList (&audio_track.stco_pages);
        audio_track.stco_pages.reset ();

        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_stts_data));
        pages.appendList (&audio_track.stts_pages);
        audio_track.stts_pages.reset ();

        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_stsz_data));
        pages.appendList (&audio_track.stsz_pages);
        audio_track.stsz_pages.reset ();

        page_pool->getFillPages (&pages, ConstMemory::forObject (audio_ctts_data));
        pages.appendList (&audio_track.ctts_pages);
        audio_track.ctts_pages.reset ();
    }

    if (got_video) {
        page_pool->getFillPages (&pages, ConstMemory::forObject (video_trak_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (video_tkhd_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (video_mdia_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (video_mdia_hdlr_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (video_minf_data));
        page_pool->getFillPages (&pages, ConstMemory::forObject (video_stbl_data));

        if (video_track.hdr_size) {
            page_pool->getFillPages (&pages, ConstMemory::forObject (video_avcc_data));
            page_pool->getFillPagesFromPages (&pages,
                                              video_track.hdr_msg,
                                              video_track.hdr_offs,
                                              video_track.hdr_size);
            video_track.hdr_msg = NULL;
        }

        page_pool->getFillPages (&pages, ConstMemory::forObject (video_stsc_data));

        page_pool->getFillPages (&pages, ConstMemory::forObject (video_stco_data));
        pages.appendList (&video_track.stco_pages);
        video_track.stco_pages.reset ();

        page_pool->getFillPages (&pages, ConstMemory::forObject (video_stts_data));
        pages.appendList (&video_track.stts_pages);
        video_track.stts_pages.reset ();

        page_pool->getFillPages (&pages, ConstMemory::forObject (video_stss_data));
        pages.appendList (&video_track.stss_pages);
        video_track.stss_pages.reset ();

        page_pool->getFillPages (&pages, ConstMemory::forObject (video_stsz_data));
        pages.appendList (&video_track.stsz_pages);
        video_track.stsz_pages.reset ();

        page_pool->getFillPages (&pages, ConstMemory::forObject (video_ctts_data));
        pages.appendList (&video_track.ctts_pages);
        video_track.ctts_pages.reset ();
    }

    if (!moov_only)
        page_pool->getFillPages (&pages, ConstMemory::forObject (mdat_data));

    if (logLevelOn (mp4mux, LogLevel::Debug)) {
        logLock ();
        log_locked__ (LogLevel::Debug, _func, "result: ", pages.data_len, " bytes:");
        // FIXME log pages.data_len bytes
        PagePool::logPages_locked (LogLevel::Debug, pages.first, 0 /* first_page_offs */, _func);
        logUnlock ();
    }

    return pages;
}

void
Mp4Muxer::pass1_mediaDesc (MediaDesc_Audio *audio_desc,
                           bool             set_audio,
                           MediaDesc_Video *video_desc,
                           bool             set_video)
{
    if (set_audio && audio_desc)
        media_desc__audio = audio_desc;

    if (set_video && video_desc)
        media_desc__video = video_desc;
}

void
Mp4Muxer::pass1_aacSequenceHeader (PagePool       * const mt_nonnull msg_page_pool,
                                   PagePool::Page * const msg,
                                   Size             const msg_offs,
                                   Size             const frame_size)
{
    if (audio_track.hdr_msg) {
        logW (mp4mux, _func, "duplicate invocation");
        page_pool->msgUnref (audio_track.hdr_msg);
    }

    msg_page_pool->msgRef (msg);

    audio_track.hdr_page_pool = msg_page_pool;
    audio_track.hdr_msg = msg;
    audio_track.hdr_offs = msg_offs;
    audio_track.hdr_size = frame_size;
}

namespace {
struct AvcSPS
{
  Uint32 id;

  Byte profile_idc;
  Byte constraint_set0_flag;
  Byte constraint_set1_flag;
  Byte constraint_set2_flag;
  Byte constraint_set3_flag;
  Byte constraint_set4_flag;
  Byte constraint_set5_flag;
  Byte level_idc;

  Uint32 chroma_format_idc;
  Byte separate_colour_plane_flag;
  Uint32 bit_depth_luma_minus8;
  Uint32 bit_depth_chroma_minus8;
  Byte qpprime_y_zero_transform_bypass_flag;

  Byte scaling_matrix_present_flag;
  Byte scaling_lists_4x4[6][16];
  Byte scaling_lists_8x8[6][64];

  Uint32 log2_max_frame_num_minus4;
  Uint32 pic_order_cnt_type;

  /* if pic_order_cnt_type == 0 */
  Uint32 log2_max_pic_order_cnt_lsb_minus4;

  /* else if pic_order_cnt_type == 1 */
  Byte delta_pic_order_always_zero_flag;
  Int32 offset_for_non_ref_pic;
  Int32 offset_for_top_to_bottom_field;
  Uint32 num_ref_frames_in_pic_order_cnt_cycle;
  Int32 offset_for_ref_frame[255];

  Uint32 num_ref_frames;
  Byte gaps_in_frame_num_value_allowed_flag;
  Uint32 pic_width_in_mbs_minus1;
  Uint32 pic_height_in_map_units_minus1;
  Byte frame_mbs_only_flag;

  Byte mb_adaptive_frame_field_flag;

  Byte direct_8x8_inference_flag;

  Byte frame_cropping_flag;

  /* if frame_cropping_flag */
  Uint32 frame_crop_left_offset;
  Uint32 frame_crop_right_offset;
  Uint32 frame_crop_top_offset;
  Uint32 frame_crop_bottom_offset;

  Byte vui_parameters_present_flag;
  /* if vui_parameters_present_flag */
//  GstH264VUIParams vui_parameters;

  /* calculated values */
  Byte chroma_array_type;
//  Uint32 max_frame_num;
  int width, height;
  int crop_rect_width, crop_rect_height;
  int crop_rect_x, crop_rect_y;
  int fps_num, fps_den;
  bool valid;
};
}

static Result extractDimensionsFromAvcSeqHdr (Array  * const arr,
                                              Size     const frame_size,
                                              Size     const offset,
                                              Uint64 * const ret_width,
                                              Uint64 * const ret_height)
{
    *ret_width  = 640;
    *ret_height = 480;

    Size sps_len = 0;
    {
        if (frame_size < offset + 3)
            return Result::Failure;

        Byte block [3];
        arr->get (offset + 0, Memory::forObject (block));

        Uint32 const num_sps = block [0] & 0x1f;
        if (num_sps == 0)
            return Result::Failure;

        Uint16 const param_len = ((Uint16) block [1] << 8) |
                                 ((Uint16) block [2] << 0);
        if (param_len < 1 || (Size) param_len + offset + 3 > frame_size)
            return Result::Failure;

        sps_len = param_len - 1;
    }

    NalReader reader (arr, sps_len, offset + 4);

    AvcSPS sps;
    sps.chroma_format_idc = 1;
    sps.separate_colour_plane_flag = 0;
    sps.bit_depth_luma_minus8 = 0;
    sps.bit_depth_chroma_minus8 = 0;
    memset (sps.scaling_lists_4x4, 16, 96);
    memset (sps.scaling_lists_8x8, 16, 384);
    // memset (&sps.vui_parameters, 0, sizeof (sps.vui_parameters));
    sps.mb_adaptive_frame_field_flag = 0;
    sps.frame_crop_left_offset = 0;
    sps.frame_crop_right_offset = 0;
    sps.frame_crop_top_offset = 0;
    sps.frame_crop_bottom_offset = 0;
    sps.delta_pic_order_always_zero_flag = 0;

    if (!reader.readUint (&sps.profile_idc, 8))          goto _failure;
    if (!reader.readUint (&sps.constraint_set0_flag, 1)) goto _failure;
    if (!reader.readUint (&sps.constraint_set1_flag, 1)) goto _failure;
    if (!reader.readUint (&sps.constraint_set2_flag, 1)) goto _failure;
    if (!reader.readUint (&sps.constraint_set3_flag, 1)) goto _failure;
    if (!reader.readUint (&sps.constraint_set4_flag, 1)) goto _failure;
    if (!reader.readUint (&sps.constraint_set5_flag, 1)) goto _failure;
    if (!reader.skip (2))                                goto _failure;
    if (!reader.readUint (&sps.level_idc, 8))            goto _failure;
    if (!reader.readUe (&sps.id))                        goto _failure;

    if (   sps.profile_idc == 100
        || sps.profile_idc == 110
        || sps.profile_idc == 122
        || sps.profile_idc == 244
        || sps.profile_idc == 44
        || sps.profile_idc == 83
        || sps.profile_idc == 86
        || sps.profile_idc == 118
        || sps.profile_idc == 128)
    {
        logD_ (_func, "special profile_idc");
        return Result::Failure;

        if (!reader.readUe (&sps.chroma_format_idc))                     goto _failure;
        if (sps.chroma_format_idc > 3)
            goto _failure;
        if (sps.chroma_format_idc == 3)
            if (!reader.readUint (&sps.separate_colour_plane_flag, 1))   goto _failure;

        if (!reader.readUe (&sps.bit_depth_luma_minus8))                 goto _failure;
        if (!reader.readUe (&sps.bit_depth_chroma_minus8))               goto _failure;
        if (!reader.readUint (&sps.qpprime_y_zero_transform_bypass_flag, 1)) goto _failure;

        if (!reader.readUint (&sps.scaling_matrix_present_flag, 1))      goto _failure;
        if (sps.scaling_matrix_present_flag) {
            unsigned const num_lists = (sps.chroma_format_idc != 3 ? 8 : 12);
            // TODO parseScalingList (num_lists);
            (void) num_lists;
        }
    }

    if (!reader.readUe (&sps.log2_max_frame_num_minus4))                 goto _failure;

    if (!reader.readUe (&sps.pic_order_cnt_type))                        goto _failure;
    if (sps.pic_order_cnt_type == 0) {
        if (!reader.readUe (&sps.log2_max_pic_order_cnt_lsb_minus4))     goto _failure;
    } else
    if (sps.pic_order_cnt_type == 1) {
        logD_ (_func, "pic_order_cnt_type == 1");
        return Result::Failure;

        if (!reader.readUint (&sps.delta_pic_order_always_zero_flag, 1)) goto _failure;
        // TODO read_se &sps.offset_for_non_ref_pic
        // TODO read_se &sps.offset_for_top_to_bottom_field
        if (!reader.readUe (&sps.num_ref_frames_in_pic_order_cnt_cycle)) goto _failure;
        // TODO sps.num_ref_frames_in_pic_order_cnt_cycle sanity check
        for (Uint32 i = 0; i < sps.num_ref_frames_in_pic_order_cnt_cycle; ++i)
            reader.readSe (&sps.offset_for_ref_frame [i]);
    }

    if (!reader.readUe (&sps.num_ref_frames))                            goto _failure;
    if (!reader.readUint (&sps.gaps_in_frame_num_value_allowed_flag, 1)) goto _failure;
    if (!reader.readUe (&sps.pic_width_in_mbs_minus1))                   goto _failure;
    if (!reader.readUe (&sps.pic_height_in_map_units_minus1))            goto _failure;
    if (!reader.readUint (&sps.frame_mbs_only_flag, 1))                  goto _failure;

    if (!sps.frame_mbs_only_flag)
        if (!reader.readUint (&sps.mb_adaptive_frame_field_flag, 1))     goto _failure;

    if (!reader.readUint (&sps.direct_8x8_inference_flag, 1))            goto _failure;
    if (!reader.readUint (&sps.frame_cropping_flag, 1))                  goto _failure;
    if (sps.frame_cropping_flag) {
        if (!reader.readUe (&sps.frame_crop_left_offset))                goto _failure;
        if (!reader.readUe (&sps.frame_crop_right_offset))               goto _failure;
        if (!reader.readUe (&sps.frame_crop_top_offset))                 goto _failure;
        if (!reader.readUe (&sps.frame_crop_bottom_offset))              goto _failure;
    }

    if (!reader.readUint (&sps.vui_parameters_present_flag, 1)) goto _failure;
    // TODO parse vui

    {
        Uint64 width  = (sps.pic_width_in_mbs_minus1 + 1) * 16;
        Uint64 height = (sps.pic_height_in_map_units_minus1 + 1) * 16 * (2 - sps.frame_mbs_only_flag);

        if (sps.frame_cropping_flag) {
            unsigned const subwc [] = { 1, 2, 2, 1 };
            unsigned const subhc [] = { 1, 2, 1, 1 };

            const unsigned crop_unit_x = subwc [sps.chroma_format_idc];
            const unsigned crop_unit_y = subhc [sps.chroma_format_idc] * (2 - sps.frame_mbs_only_flag);

            width  -= (sps.frame_crop_left_offset + sps.frame_crop_right_offset)  * crop_unit_x;
            height -= (sps.frame_crop_top_offset  + sps.frame_crop_bottom_offset) * crop_unit_y;
        }

        logD_ (_func, "width ", width, ", height ", height);

        *ret_width  = width;
        *ret_height = height;
    }

    return Result::Success;

_failure:
    return Result::Failure;
}

void
Mp4Muxer::pass1_avcSequenceHeader (PagePool       * const mt_nonnull msg_page_pool,
                                   PagePool::Page * const msg,
                                   Size             const msg_offs,
                                   Size             const frame_size)
{
    if (video_track.hdr_msg) {
        logW (mp4mux, _func, "duplicate invocation");
        page_pool->msgUnref (video_track.hdr_msg);
    }

    msg_page_pool->msgRef (msg);

    video_track.hdr_page_pool = msg_page_pool;
    video_track.hdr_msg = msg;
    video_track.hdr_offs = msg_offs;
    video_track.hdr_size = frame_size;

    if (frame_size > 5) {
        Uint64 width  = 0;
        Uint64 height = 0;

        PagePool::PageListArray arr (msg, msg_offs, frame_size);
        if (extractDimensionsFromAvcSeqHdr (&arr, frame_size, 5 /* offset */, &width, &height)) {
            avc_hdr_width  = (Uint32) width;
            avc_hdr_height = (Uint32) height;
        }
    }
}

void
Mp4Muxer::processFrame (TrackInfo * const mt_nonnull track,
                        Time        const pts_nanosec,
                        Time        const dts_nanosec,
                        Size        const frame_size,
                        bool        const is_sync_sample)
{
    logD (mp4mux, _func, "pts ", pts_nanosec, ", dts ", dts_nanosec, ", len ", frame_size);

    ++track->num_frames;
    track->total_frame_size += frame_size;

    {
        Byte const stsz_entry [] = {
            (Byte) (frame_size >> 24),
            (Byte) (frame_size >> 16),
            (Byte) (frame_size >>  8),
            (Byte) (frame_size >>  0)
        };
        page_pool->getFillPages (&track->stsz_pages, ConstMemory::forObject (stsz_entry));
        track->stsz_pos += sizeof (stsz_entry);
        logD (mp4mux, _func, "stsz_pos: ", track->stsz_pos);
    }

    if (is_sync_sample) {
        Byte const stss_entry [] = {
            (Byte) (track->num_frames >> 24),
            (Byte) (track->num_frames >> 16),
            (Byte) (track->num_frames >>  8),
            (Byte) (track->num_frames >>  0)
        };
        page_pool->getFillPages (&track->stss_pages, ConstMemory::forObject (stss_entry));
        track->stss_pos += sizeof (stss_entry);
        ++track->num_stss_entries;
    }

    Time const pts = pts_nanosec / (1000000 / 3);
    if (derive_dts_from_pts) {
//#warning TODO Implement the new "perfect" DTS recovery scheme with post-processing:
//#warning          1. DTS sequence is equivalent to sorted PTS sequence (fill-time local sort);
//#warning          2. Shift PTS timestamps so that PTS >= DTS.

        Time const dts = pts;

        // Canonical:
        //    ts   0 3 1 2 4
        //   dts   0 1 2 3 4
        //   pts   1 4 2 3 5
        //   cto   1 3 0 0 1
        //
        // We do:
        //    ts   0 3 1 2 4
        //   dts   0 1 1 2 4
        //   pts   0 3 1 2 4
        //  stts   1 0 1 2 .
        //   cto   0 2 0 0 0
        //
        //    ts   0  3  1
        //   dts   0  3
        //  stts  +0 +2
        //  ctts   0  0
        //
        //    ts   0  3  1  2  4
        //   dts   0  1  1  2  4
        //  stts  +0 +1 +0 +1 +2
        //  ctts   0  2  0  0  0
        //
        //         0 3 2 1 4
        //   dts   0 1 1 1 4
        //  stts   1 0 0 3 .
        //  ctts   0 2 1 0 0
        //
        //         0 3 2
        //   dts   0 2 2
        //  stts   2 0 .
        //  ctts   0 1 0
        //
        // Как строить таблицы?
        //
        // Если dts < prv_dts, то по stts идём назад и протягиваем нули.
        // При этом в ctts меняем значения в соответствии с изменениями в stts.
        //
        // Ещё один момент: dts начинаются с 0, поэтому может потребоваться сдвиг.
        //
        // Сейчас один кадр -> одна запись в stts и ctts.
        //
        // 3 0 1
        // 0 0 1
        // 0 1
        //
        // 3 1 2
        // 1 1 2 => 0 0 2 (новый минимум)
        // 0 0 1

        if (pts < track->prv_pts) {
            PagePool::PageListArray stts_arr (track->stts_pages.first, 0 /* offset*/, track->stts_pos /* data_len */);
            PagePool::PageListArray ctts_arr (track->ctts_pages.first, 0 /* offset*/, track->ctts_pos /* data_len */);

            Byte stts_entry [8];
            Byte ctts_entry [8];

            if (pts < track->min_pts) {
//#warning Walking backwards is inefficient for PageListArray. Walk forwards instead.
                for (Size ctts_i = track->ctts_pos; ctts_i > 0; ctts_i -= sizeof (ctts_entry)) {
                    ctts_arr.get (ctts_i - sizeof (ctts_entry), Memory::forObject (ctts_entry));
                    Uint32 const ctts_val = (track->min_pts - pts) + (((Uint32) ctts_entry [4] << 24) |
                                                                      ((Uint32) ctts_entry [5] << 16) |
                                                                      ((Uint32) ctts_entry [6] <<  8) |
                                                                      ((Uint32) ctts_entry [7] <<  0));
                    ctts_entry [4] = (Byte) (ctts_val >> 24);
                    ctts_entry [5] = (Byte) (ctts_val >> 16);
                    ctts_entry [6] = (Byte) (ctts_val >>  8);
                    ctts_entry [7] = (Byte) (ctts_val >>  0);
                    ctts_arr.set (track->ctts_pos - sizeof (ctts_entry), ConstMemory::forObject (ctts_entry));
                }

                track->min_pts = pts;
            }

            Size stts_i = track->stts_pos;
            Size ctts_i = track->ctts_pos;

            Time cur_dts = track->prv_pts;
            for (;;) {
                assert (stts_i != 0);

                stts_arr.get (stts_i - sizeof (stts_entry), Memory::forObject (stts_entry));
                Uint32 const stts_val = ((Uint32) stts_entry [4] << 24) |
                                        ((Uint32) stts_entry [5] << 16) |
                                        ((Uint32) stts_entry [6] <<  8) |
                                        ((Uint32) stts_entry [7] <<  0);

                ctts_arr.get (ctts_i - sizeof (ctts_entry), Memory::forObject (ctts_entry));
                Uint32 const ctts_val = ((Uint32) ctts_entry [4] << 24) |
                                        ((Uint32) ctts_entry [5] << 16) |
                                        ((Uint32) ctts_entry [6] <<  8) |
                                        ((Uint32) ctts_entry [7] <<  0);

                bool last = false;
                Uint32 new_ctts_val;
                if (stts_val >= cur_dts - dts) {
                    last = true;

                    Uint32 const new_stts_val = stts_val - (cur_dts - dts);
                    stts_entry [4] = (Byte) (new_stts_val >> 24);
                    stts_entry [5] = (Byte) (new_stts_val >> 16);
                    stts_entry [6] = (Byte) (new_stts_val >>  8);
                    stts_entry [7] = (Byte) (new_stts_val >>  0);

                    new_ctts_val = ctts_val + (cur_dts - dts);
                } else {
                    stts_entry [4] = 0;
                    stts_entry [5] = 0;
                    stts_entry [6] = 0;
                    stts_entry [7] = 0;

                    new_ctts_val = stts_val;
                }

                ctts_entry [4] = (Byte) (new_ctts_val >> 24);
                ctts_entry [5] = (Byte) (new_ctts_val >> 16);
                ctts_entry [6] = (Byte) (new_ctts_val >>  8);
                ctts_entry [7] = (Byte) (new_ctts_val >>  0);

                stts_arr.set (stts_i - sizeof (stts_entry), ConstMemory::forObject (stts_entry));
                ctts_arr.set (ctts_i - sizeof (ctts_entry), ConstMemory::forObject (ctts_entry));

                if (last)
                    break;

                cur_dts -= stts_val;

                stts_i -= sizeof (stts_entry);
                ctts_i -= sizeof (ctts_entry);
            }
        }

        {
            Byte const ctts_entry [] = {
                0, 0, 0, 1,
                0, 0, 0, 0
            };
            page_pool->getFillPages (&track->ctts_pages, ConstMemory::forObject (ctts_entry));
            track->ctts_pos += sizeof (ctts_entry);
        }
    } else {
        Time const pts = pts_nanosec / (1000000 / 3);
        Time const dts = dts_nanosec / (1000000 / 3);

        Time const ctts_offset = pts - dts;
        {
            Byte const ctts_entry [] = {
                0, 0, 0, 1,
                (Byte) (ctts_offset >> 24),
                (Byte) (ctts_offset >> 16),
                (Byte) (ctts_offset >>  8),
                (Byte) (ctts_offset >>  0)
            };
            page_pool->getFillPages (&track->ctts_pages, ConstMemory::forObject (ctts_entry));
            track->ctts_pos += sizeof (ctts_entry);
        }
    }

    if (track->num_frames > 1) {
        Byte const stts_entry [] = {
            0, 0, 0, 1,
            (Byte) ((pts - track->prv_pts) >> 24),
            (Byte) ((pts - track->prv_pts) >> 16),
            (Byte) ((pts - track->prv_pts) >>  8),
            (Byte) ((pts - track->prv_pts) >>  0)
        };

        page_pool->getFillPages (&track->stts_pages, ConstMemory::forObject (stts_entry));
        track->stts_pos += sizeof (stts_entry);
    }

    {
        Byte const stco_entry [] = {
            (Byte) (mdat_pos >> 24),
            (Byte) (mdat_pos >> 16),
            (Byte) (mdat_pos >>  8),
            (Byte) (mdat_pos >>  0),
        };
        page_pool->getFillPages (&track->stco_pages, ConstMemory::forObject (stco_entry));
        track->stco_pos += sizeof (stco_entry);
    }

    track->prv_pts = pts;

    mdat_pos += frame_size;
}

void
Mp4Muxer::pass1_frame (FrameType const frame_type,
                       Time      const pts_nanosec,
                       Time      const dts_nanosec,
                       Size      const frame_size,
                       bool      const is_sync_sample)
{
    if (frame_type == FrameType_Audio)
        processFrame (&audio_track, pts_nanosec, dts_nanosec, frame_size, is_sync_sample);
    else
        processFrame (&video_track, pts_nanosec, dts_nanosec, frame_size, is_sync_sample);
}

void
Mp4Muxer::finalizeTrack (TrackInfo * const mt_nonnull track,
                         Time        const duration_millisec)
{
    if (track->num_frames > 0) {
        // (total duration) - (last frame timestamp)
        Time last_duration = 0;
        if (duration_millisec * 3 > track->prv_pts)
            last_duration = duration_millisec * 3 - track->prv_pts;

        Byte const stts_entry [] = {
            0, 0, 0, 1,
            (Byte) (last_duration >> 24),
            (Byte) (last_duration >> 16),
            (Byte) (last_duration >>  8),
            (Byte) (last_duration >>  0),
        };

        page_pool->getFillPages (&track->stts_pages, ConstMemory::forObject (stts_entry));
        track->stts_pos += sizeof (stts_entry);
    }
}

PagePool::PageListInfo
Mp4Muxer::pass1_complete (Time const duration_millisec,
                          bool const moov_only,
                          bool const moov_tail)
{
    finalizeTrack (&audio_track, duration_millisec);
    finalizeTrack (&video_track, duration_millisec);

    return writeMoovAtom (duration_millisec, moov_only, moov_tail);
}

void
Mp4Muxer::TrackInfo::clear (PagePool * const mt_nonnull page_pool)
{
    if (hdr_page_pool) {
        hdr_page_pool->msgUnref (hdr_msg);
        hdr_page_pool = NULL;
        hdr_msg = NULL;
    }

    page_pool->msgUnref (stsz_pages.first);
    stsz_pages.reset ();

    page_pool->msgUnref (stss_pages.first);
    stss_pages.reset ();

    page_pool->msgUnref (stts_pages.first);
    stts_pages.reset ();

    page_pool->msgUnref (ctts_pages.first);
    ctts_pages.reset ();

    page_pool->msgUnref (stco_pages.first);
    stco_pages.reset ();
}

void
Mp4Muxer::clear ()
{
    audio_track.clear (page_pool);
    video_track.clear (page_pool);

    media_desc__audio = NULL;
    media_desc__video = NULL;
}

void
Mp4Muxer::init (PagePool * const mt_nonnull page_pool,
                bool       const derive_dts_from_pts)
{
    this->page_pool = page_pool;
    this->derive_dts_from_pts = derive_dts_from_pts;
}

Mp4Muxer::~Mp4Muxer ()
{
    clear ();
}

}

