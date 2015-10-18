/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/media_message.h>


namespace Moment {

Size
AudioFrameType::toString_ (Memory const &mem,
                           Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case RawData:
            return toString (mem, "RawData");
        case AacSequenceHeader:
            return toString (mem, "AacSequenceHeader");
        case SpeexHeader:
            return toString (mem, "SpeexHeader");
    }

    unreachable ();
    return 0;
}

Size
VideoFrameType::toString_ (Memory const &mem,
                           Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case KeyFrame:
            return toString (mem, "KeyFrame");
        case InterFrame:
            return toString (mem, "InterFrame");
        case DisposableInterFrame:
            return toString (mem, "DisposableInterFrame");
        case GeneratedKeyFrame:
            return toString (mem, "GeneratedKeyFrame");
        case CommandFrame:
            return toString (mem, "CommandFrame");
        case AvcSequenceHeader:
            return toString (mem, "AvcSequenceHeader");
        case AvcEndOfSequence:
            return toString (mem, "AvcEndOfSequence");
        case RtmpSetMetaData:
            return toString (mem, "RtmpSetMetaData");
        case RtmpClearMetaData:
            return toString (mem, "RtmpClearMetaData");
        case Last:
            return toString (mem, "Last");
    }

    unreachable ();
    return 0;
}

Size
AudioCodecId::toString_ (Memory const &mem,
                         Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case LinearPcmPlatformEndian:
            return toString (mem, "LinearPcmPlatformEndian");
        case ADPCM:
            return toString (mem, "ADPCM");
        case MP3:
            return toString (mem, "MP3");
        case LinearPcmLittleEndian:
            return toString (mem, "LinearPcmLittleEndian");
        case Nellymoser_16kHz_mono:
            return toString (mem, "Nellymoser_16kHz_mono");
        case Nellymoser_8kHz_mono:
            return toString (mem, "Nellymoser_8kHz_mono");
        case Nellymoser:
            return toString (mem, "Nellymoser");
        case G711ALaw:
            return toString (mem, "G711ALaw");
        case G711MuLaw:
            return toString (mem, "G711MuLaw");
        case AAC:
            return toString (mem, "AAC");
        case Speex:
            return toString (mem, "Speex");
        case MP3_8kHz:
            return toString (mem, "MP3_8kHz");
        case DeviceSpecific:
            return toString (mem, "DeviceSpecific");
        case Last:
            return toString (mem, "Last");
    }

    unreachable ();
    return 0;
}

Size
VideoCodecId::toString_ (Memory const &mem,
                         Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case SorensonH263:
            return toString (mem, "SorensonH263");
        case ScreenVideo:
            return toString (mem, "ScreenVideo");
        case ScreenVideoV2:
            return toString (mem, "ScreenVideoV2");
        case VP6:
            return toString (mem, "VP6");
        case VP6Alpha:
            return toString (mem, "VP6Alpha");
        case AVC:
            return toString (mem, "AVC");
        case Last:
            return toString (mem, "Last");
    }

    unreachable ();
    return 0;
}

VideoFrameType
VideoFrameType::fromFlvFrameType (Byte const flv_frame_type)
{
    switch (flv_frame_type) {
        case 1:
            return KeyFrame;
        case 2:
            return InterFrame;
        case 3:
            return DisposableInterFrame;
        case 4:
            return GeneratedKeyFrame;
        case 5:
            return CommandFrame;
    }

    return Unknown;
}

Byte
VideoFrameType::toFlvFrameType () const
{
    switch (value) {
        case Unknown:
            return 0;
        case AvcSequenceHeader:
        case AvcEndOfSequence:
        case KeyFrame:
            return 1;
        case InterFrame:
            return 2;
        case DisposableInterFrame:
            return 3;
        case GeneratedKeyFrame:
            return 4;
        case CommandFrame:
            return 5;
        case RtmpSetMetaData:
        case RtmpClearMetaData:
        case Last:
            unreachable ();
    }

    unreachable ();
    return 0;
}

AudioCodecId
AudioCodecId::fromFlvCodecId (Byte const flv_codec_id)
{
    switch (flv_codec_id) {
        case 0:
            return LinearPcmPlatformEndian;
        case 1:
            return ADPCM;
        case 2:
            return MP3;
        case 3:
            return LinearPcmLittleEndian;
        case 4:
            return Nellymoser_16kHz_mono;
        case 5:
            return Nellymoser_8kHz_mono;
        case 6:
            return Nellymoser;
        case 7:
            return G711ALaw;
        case 8:
            return G711MuLaw;
        case 10:
            return AAC;
        case 11:
            return Speex;
        case 14:
            return MP3_8kHz;
        case 15:
            return DeviceSpecific;
    }

    return Unknown;
}

Byte
AudioCodecId::toFlvCodecId () const
{
    switch (value) {
        case Unknown:
            return (Byte) -1;
        case LinearPcmPlatformEndian:
            return 0;
        case ADPCM:
            return 1;
        case MP3:
            return 2;
        case LinearPcmLittleEndian:
            return 3;
        case Nellymoser_16kHz_mono:
            return 4;
        case Nellymoser_8kHz_mono:
            return 5;
        case Nellymoser:
            return 6;
        case G711ALaw:
            return 7;
        case G711MuLaw:
            return 8;
        case AAC:
            return 10;
        case Speex:
            return 11;
        case MP3_8kHz:
            return 14;
        case DeviceSpecific:
            return 15;
        case Last:
            unreachable ();
    }

    unreachable ();
    return (Byte) -1;
}

VideoCodecId
VideoCodecId::fromFlvCodecId (Byte const flv_codec_id)
{
    switch (flv_codec_id) {
        case 2:
            return SorensonH263;
        case 3:
            return ScreenVideo;
        case 4:
            return VP6;
        case 5:
            return VP6Alpha;
        case 6:
            return ScreenVideoV2;
        case 7:
            return AVC;
    }

    return Unknown;
}

Byte
VideoCodecId::toFlvCodecId () const
{
    switch (value) {
        case Unknown:
            return 0;
        case SorensonH263:
            return 2;
        case ScreenVideo:
            return 3;
        case VP6:
            return 4;
        case VP6Alpha:
            return 5;
        case ScreenVideoV2:
            return 6;
        case AVC:
            return 7;
        case Last:
            unreachable ();
    }

    unreachable ();
    return 0;
}

Size
MediaMessage::toString_ (Memory const &mem,
                         Format const & /* fmt */) const
{
    return printToString (
            mem,
            "MediaMessage "
            "pts ", pts_nanosec, ", "
            "dts ", dts_nanosec, ", "
            "len ", msg_len);
}

Size
AudioMessage::toString_ (Memory const &mem,
                         Format const & /* fmt */) const
{
    return printToString (
            mem,
            "AudioMessage "
            "pts ",      pts_nanosec, ", "
            "dts ",      dts_nanosec, ", "
            "len ",      msg_len, ", "
            "frame ",    frame_type, ", "
            "codec ",    codec_id, ", "
            "rate ",     rate, ", "
            "channels ", channels);
}

Size
VideoMessage::toString_ (Memory const &mem,
                         Format const & /* fmt */) const
{
    return printToString (
            mem,
            "VideoMessage "
            "pts ",   pts_nanosec, ", "
            "dts ",   dts_nanosec, ", "
            "len ",   msg_len, ", "
            "frame ", frame_type, ", "
            "codec ", codec_id);
}

Size
MediaDescMessage::toString_ (Memory const &mem,
                             Format const & /* fmt */) const
{
    if (!media_desc__audio && !media_desc__video)
        return printToString (mem, "MediaDescMessage empty");

    if (media_desc__audio && media_desc__video)
        return printToString (mem, "MediaDescMessage [", *media_desc__audio, "] [", *media_desc__video, "]");

    if (media_desc__audio)
        return printToString (mem, "MediaDescMessage [", *media_desc__audio, "]");

    if (media_desc__video)
        return printToString (mem, "MediaDescMessage [", *media_desc__video, "]");

    return 0;
}

}

