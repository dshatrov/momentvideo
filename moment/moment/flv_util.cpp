/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/flv_util.h>


namespace Moment {

unsigned fillFlvAudioHeader (AudioMessage * const mt_nonnull audio_msg,
                             Memory const mem)
{
    assert (mem.len() >= 2);

    Byte audio_hdr = 0;
    Byte audio_hdr_ext = 0;
    unsigned audio_hdr_len = 1;

#if 0
    switch (audio_msg->frame_type) {
        case Unknown:
            assert (0 && "Unknown audio frame type");
            break;
        case RawData:
        case AacSequenceHeader:
        case SpeexHeader:
          // Fall-through
            break;
    }
#endif

    switch (audio_msg->codec_id) {
        case AudioCodecId::Unknown:
        case AudioCodecId::Last:
            logD_ (_func, "unknown audio codec id");
            return 0;
        case AudioCodecId::LinearPcmPlatformEndian:
            audio_hdr = 0x00;
            break;
        case AudioCodecId::ADPCM:
	    audio_hdr = 0x10;
            break;
        case AudioCodecId::MP3:
            audio_hdr = 0x20;
            break;
        case AudioCodecId::LinearPcmLittleEndian:
	    audio_hdr = 0x30;
            break;
        case AudioCodecId::Nellymoser_16kHz_mono:
            audio_hdr = 0x40;
            break;
        case AudioCodecId::Nellymoser_8kHz_mono:
            audio_hdr = 0x50;
            break;
        case AudioCodecId::Nellymoser:
	    audio_hdr = 0x60;
            break;
        case AudioCodecId::G711ALaw:
	    audio_hdr = 0x70;
            break;
        case AudioCodecId::G711MuLaw:
	    audio_hdr = 0x80;
            break;
        case AudioCodecId::AAC:
            audio_hdr = 0xa0;

            if (audio_msg->frame_type == AudioFrameType::AacSequenceHeader)
                audio_hdr_ext = 0; // AAC sequence header
            else
                audio_hdr_ext = 1; // AAC raw

            audio_hdr_len = 2;
            break;
        case AudioCodecId::Speex:
	    audio_hdr = 0xb0;
            break;
        case AudioCodecId::MP3_8kHz:
            audio_hdr = 0xe0;
            break;
        case AudioCodecId::DeviceSpecific:
            audio_hdr = 0xf0;
            break;
    }

    unsigned rate = audio_msg->rate;
    unsigned channels = audio_msg->channels;

    if (audio_msg->codec_id == AudioCodecId::AAC) {
      // FLV spec forces these values.
        rate = 44100;
        channels = 2;
    } else
    if (audio_msg->codec_id == AudioCodecId::Speex) {
      // FLV spec forces these values.
        rate = 5512;
        channels = 1;
    }

//    if (audio_msg->codec_id != AudioCodecId::G711ALaw &&
//        audio_msg->codec_id != AudioCodecId::G711MuLaw)
    {
        audio_hdr |= 0x02; // 16-bit samples
    }

    if (channels > 1)
        audio_hdr |= 1; // stereo

    switch (rate) {
        case 8000:
            if (audio_msg->codec_id == AudioCodecId::MP3) {
                audio_hdr &= 0x0f;
                audio_hdr |= 0xe0; // MP3 8 kHz
            }

            audio_hdr |= 0x04; // 11 kHz
            break;
        case 5512:
        case 5513:
            audio_hdr |= 0x00; // 5.5 kHz
            break;
        case 11025:
        case 16000:
            audio_hdr |= 0x04; // 11 kHz
            break;
        case 22050:
            audio_hdr |= 0x08; // 22 kHz
            break;
        case 44100:
            audio_hdr |= 0x0c; // 44 kHz
            break;
        default:
            audio_hdr |= 0x0c; // 44 kHz
            break;
    }

    mem.mem() [0] = audio_hdr;
    mem.mem() [1] = audio_hdr_ext;

//    logD_ (_func, "audio_hdr: 0x", fmt_hex, audio_hdr, ", audio_hdr_len: ", audio_hdr_len);

    return audio_hdr_len;
}

unsigned fillFlvVideoHeader (VideoMessage * const mt_nonnull video_msg,
                             Memory const mem)
{
//    logD_ (_func, "codec_id: ", video_msg->codec_id, ", frame_type: ", video_msg->frame_type);

    assert (mem.len() >= 5);

    Byte video_hdr [5] = { 0, 0, 0, 0, 0 };
    Size video_hdr_len = 1;
    switch (video_msg->codec_id) {
        case VideoCodecId::Unknown:
        case VideoCodecId::Last:
            logW_ (_func, "unknown video codec id");
            return 0;
        case VideoCodecId::SorensonH263:
            video_hdr [0] = 0x02;
            break;
        case VideoCodecId::ScreenVideo:
            video_hdr [0] = 0x03;
            break;
        case VideoCodecId::ScreenVideoV2:
            video_hdr [0] = 0x06;
            break;
        case VideoCodecId::VP6:
            video_hdr [0] = 0x04;
            break;
        case VideoCodecId::VP6Alpha:
            video_hdr [0] = 0x05;
            break;
        case VideoCodecId::AVC:
            video_hdr [0] = 0x07;

            video_hdr [1] = 1; // AVC NALU

            // Composition time offset
            video_hdr [2] = 0;
            video_hdr [3] = 0;
            video_hdr [4] = 0;

            video_hdr_len = 5;
            break;
    }

    switch (video_msg->frame_type) {
        case VideoFrameType::Unknown:
        case VideoFrameType::Last:
            logW_ (_func, "unknown video frame type");
            break;
        case VideoFrameType::KeyFrame:
            video_hdr [0] |= 0x10;
            break;
        case VideoFrameType::InterFrame:
            video_hdr [0] |= 0x20;
            break;
        case VideoFrameType::DisposableInterFrame:
            video_hdr [0] |= 0x30;
            break;
        case VideoFrameType::GeneratedKeyFrame:
            video_hdr [0] |= 0x40;
            break;
        case VideoFrameType::CommandFrame:
            video_hdr [0] |= 0x50;
            break;
        case VideoFrameType::AvcSequenceHeader:
            video_hdr [0] |= 0x10; // AVC seekable frame
            video_hdr [1] = 0;     // AVC sequence header
            break;
        case VideoFrameType::AvcEndOfSequence:
            video_hdr [0] |= 0x10; // AVC seekable frame
            video_hdr [1] = 2;     // AVC end of sequence
            break;
        case VideoFrameType::RtmpSetMetaData:
        case VideoFrameType::RtmpClearMetaData:
            assert (0 && "unexpected frame type");
    }

    memcpy (mem.mem(), video_hdr, video_hdr_len);
    return video_hdr_len;
}

}

