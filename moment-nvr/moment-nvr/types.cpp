/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/types.h>


namespace MomentNvr {

Size
AudioRecordFrameType::toString_ (Memory const &mem,
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

AudioRecordFrameType
toAudioRecordFrameType (AudioFrameType const ftype)
{
    switch (ftype) {
        case AudioFrameType::RawData:
            return  AudioRecordFrameType::RawData;
        case AudioFrameType::AacSequenceHeader:
            return  AudioRecordFrameType::AacSequenceHeader;
        case AudioFrameType::SpeexHeader:
            return  AudioRecordFrameType::SpeexHeader;
        default:
            return  AudioRecordFrameType::Unknown;
    }
    unreachable ();
    return AudioRecordFrameType::Unknown;
}

AudioFrameType
toAudioFrameType (unsigned const ftype)
{
    switch (ftype) {
        case AudioRecordFrameType::RawData:
            return AudioFrameType::RawData;
        case AudioRecordFrameType::AacSequenceHeader:
            return AudioFrameType::AacSequenceHeader;
        case AudioRecordFrameType::SpeexHeader:
            return AudioFrameType::SpeexHeader;
        default:
            return AudioFrameType::Unknown;
    }
    unreachable ();
    return AudioFrameType::Unknown;
}

Size
VideoRecordFrameType::toString_ (Memory const &mem,
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
        case AvcSequenceHeader:
            return toString (mem, "AvcSequenceHeader");
        case AvcEndOfSequence:
            return toString (mem, "AvcEndOfSequence");
    }
    unreachable ();
    return 0;
}

VideoRecordFrameType
toVideoRecordFrameType (VideoFrameType const ftype)
{
    switch (ftype) {
        case VideoFrameType::KeyFrame:
            return  VideoRecordFrameType::KeyFrame;
        case VideoFrameType::InterFrame:
            return  VideoRecordFrameType::InterFrame;
        case VideoFrameType::DisposableInterFrame:
            return  VideoRecordFrameType::DisposableInterFrame;
        case VideoFrameType::AvcSequenceHeader:
            return  VideoRecordFrameType::AvcSequenceHeader;
        case VideoFrameType::AvcEndOfSequence:
            return  VideoRecordFrameType::AvcEndOfSequence;
        default:
            return  VideoRecordFrameType::Unknown;
    }
    unreachable ();
    return VideoRecordFrameType::Unknown;
}

VideoFrameType
toVideoFrameType (unsigned const ftype)
{
    switch (ftype) {
        case VideoRecordFrameType::KeyFrame:
            return VideoFrameType::KeyFrame;
        case VideoRecordFrameType::InterFrame:
            return VideoFrameType::InterFrame;
        case VideoRecordFrameType::DisposableInterFrame:
            return VideoFrameType::DisposableInterFrame;
        case VideoRecordFrameType::AvcSequenceHeader:
            return VideoFrameType::AvcSequenceHeader;
        case VideoRecordFrameType::AvcEndOfSequence:
            return VideoFrameType::AvcEndOfSequence;
        default:
            return VideoFrameType::Unknown;
    }
    unreachable ();
    return VideoFrameType::Unknown;
}

Size
AudioRecordCodecId::toString_ (Memory const &mem,
                               Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case AAC:
            return toString (mem, "AAC");
        case MP3:
            return toString (mem, "MP3");
        case Speex:
            return toString (mem, "Speex");
        case ADPCM:
            return toString (mem, "ADPCM");
        case LinearPcmLittleEndian:
            return toString (mem, "LinearPcmLittleEndian");
    }
    unreachable ();
    return 0;
}

AudioRecordCodecId
toAudioRecordCodecId (AudioCodecId const cid)
{
    switch (cid) {
        case AudioCodecId::AAC:
            return  AudioRecordCodecId::AAC;
        case AudioCodecId::MP3:
            return  AudioRecordCodecId::MP3;
        case AudioCodecId::Speex:
            return  AudioRecordCodecId::Speex;
        case AudioCodecId::ADPCM:
            return  AudioRecordCodecId::ADPCM;
        case AudioCodecId::LinearPcmLittleEndian:
            return  AudioRecordCodecId::LinearPcmLittleEndian;
        default:
            return  AudioRecordCodecId::Unknown;
    }
    unreachable ();
    return AudioRecordCodecId::Unknown;
}

Size
VideoRecordCodecId::toString_ (Memory const &mem,
                               Format const & /* fmt */) const
{
    switch (value) {
        case Unknown:
            return toString (mem, "Unknown");
        case AVC:
            return toString (mem, "AVC");
        case SorensonH263:
            return toString (mem, "SorensonH263");
    }
    unreachable ();
    return 0;
}

VideoRecordCodecId
toVideoRecordCodecId (VideoCodecId const cid)
{
    switch (cid) {
        case VideoCodecId::AVC:
            return  VideoRecordCodecId::AVC;
        case VideoCodecId::SorensonH263:
            return  VideoRecordCodecId::SorensonH263;
        default:
            return  VideoRecordCodecId::Unknown;
    }
    unreachable ();
    return VideoRecordCodecId::Unknown;
}

}

