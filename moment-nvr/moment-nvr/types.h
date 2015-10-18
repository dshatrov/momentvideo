/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__TYPES__H__
#define MOMENT_NVR__TYPES__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace Moment;

struct AudioRecordFrameType
{
    enum Value {
        Unknown           = 0,
        RawData           = 1,
        AacSequenceHeader = 2,
        SpeexHeader       = 3
    };
    operator Value () const { return value; }
    AudioRecordFrameType (Value const value) : value (value) {}
    AudioRecordFrameType () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;
};

AudioRecordFrameType toAudioRecordFrameType (AudioFrameType ftype);
AudioFrameType       toAudioFrameType       (unsigned ftype);

struct VideoRecordFrameType
{
    enum Value {
        Unknown              = 0,
        KeyFrame             = 1,
        InterFrame           = 2,
        DisposableInterFrame = 3,
        AvcSequenceHeader    = 4,
        AvcEndOfSequence     = 5
    };
    operator Value () const { return value; }
    VideoRecordFrameType (Value const value) : value (value) {}
    VideoRecordFrameType () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;
};

VideoRecordFrameType toVideoRecordFrameType (VideoFrameType ftype);
VideoFrameType       toVideoFrameType       (unsigned ftype);

struct AudioRecordCodecId
{
    enum Value {
        Unknown               = 0,
        AAC                   = 1,
        MP3                   = 2,
        Speex                 = 3,
        ADPCM                 = 4,
        LinearPcmLittleEndian = 5
    };
    operator Value () const { return value; }
    AudioRecordCodecId (Value const value) : value (value) {}
    AudioRecordCodecId () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;
};

AudioRecordCodecId toAudioRecordCodecId (AudioCodecId cid);

struct VideoRecordCodecId
{
    enum Value {
        Unknown      = 0,
        AVC          = 1,
        SorensonH263 = 2
    };
    operator Value () const { return value; }
    VideoRecordCodecId (Value const value) : value (value) {}
    VideoRecordCodecId () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;
};

VideoRecordCodecId toVideoRecordCodecId (VideoCodecId cid);

}


#endif /* MOMENT_NVR__TYPES__H__ */

