/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MEDIA_MESSAGE__H__
#define MOMENT__MEDIA_MESSAGE__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class AudioFrameType
{
  public:
    enum Value {
        Unknown = 0,
        RawData,
        AacSequenceHeader,
        SpeexHeader
    };
    operator Value () const { return value; }
    AudioFrameType (Value const value) : value (value) {}
    AudioFrameType () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;

  public:
    bool hasTimestamp () const { return isAudioData(); }

    bool isAudioData () const { return value == RawData; }

    bool isAudioCodecData () const
    {
        return    value == AacSequenceHeader
               || value == SpeexHeader;
    }
};

class VideoFrameType
{
  public:
    enum Value {
        Unknown = 0,
        KeyFrame,             // for AVC, a seekable frame
        InterFrame,           // for AVC, a non-seekable frame
        DisposableInterFrame, // H.264 only
        GeneratedKeyFrame,    // reserved for server use (according to FLV format spec.)
        CommandFrame,         // video info / command frame,
        AvcSequenceHeader,
        AvcEndOfSequence,
      // The following message types should be sent to clients as RTMP
      // command messages.
        RtmpSetMetaData,
        RtmpClearMetaData,
        Last
    };
    operator Value () const { return value; }
    VideoFrameType (Value const value) : value (value) {}
    VideoFrameType () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;

  public:
    static VideoFrameType fromFlvFrameType (Byte flv_frame_type);

    bool hasTimestamp () const { return isVideoData(); }

    bool isVideoData () const
    {
        return value == KeyFrame             ||
               value == InterFrame           ||
               value == DisposableInterFrame ||
               value == GeneratedKeyFrame;
    }

    bool isVideoCodecData () const
    {
        return value == AvcSequenceHeader;
    }

    bool isKeyFrame () const
        { return value == KeyFrame || value == GeneratedKeyFrame; }

    bool isInterFrame () const
        { return value == InterFrame || value == DisposableInterFrame; }

    Byte toFlvFrameType () const;
};

class AudioCodecId
{
  public:
    enum Value {
        Unknown = 0,
        LinearPcmPlatformEndian,
        ADPCM,
        MP3,
        LinearPcmLittleEndian,
        Nellymoser_16kHz_mono,
        Nellymoser_8kHz_mono,
        Nellymoser,
        G711ALaw,      // reserved
        G711MuLaw,     // reserved
        AAC,
        Speex,
        MP3_8kHz,      // reserved
        DeviceSpecific, // reserved
        Last
    };
    operator Value () const { return value; }
    AudioCodecId (Value const value) : value (value) {}
    AudioCodecId () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;

  public:
    static AudioCodecId fromFlvCodecId (Byte flv_codec_id);

    Byte toFlvCodecId () const;
};

class VideoCodecId
{
  public:
    enum Value {
        Unknown = 0,
        SorensonH263,  // Sorenson H.263
        ScreenVideo,   // Screen video
        ScreenVideoV2, // Screen video version 2
        VP6,           // On2 VP6
        VP6Alpha,      // On2 VP6 with alpha channel
        AVC,           // h.264 / AVC
        Last
    };
    operator Value () const { return value; }
    VideoCodecId (Value const value) : value (value) {}
    VideoCodecId () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;

  public:
    static VideoCodecId fromFlvCodecId (Byte flv_codec_id);

    Byte toFlvCodecId () const;
};

// Must be copyable.
class MediaMessage
{
  public:
    enum Type
    {
        Type_None,
        Type_Audio,
        Type_Video,
        Type_MediaDesc
    };

    // non-const to allow default operator=
    Type msg_type;

    Uint64 pts_nanosec;
    Uint64 dts_nanosec;

    PagePool *page_pool;
    PagePool::PageListHead page_list;
    Size msg_len;
    Size msg_offset;

    void seize ()
    {
        if (page_pool)
            page_pool->msgRef (page_list.first);
    }

    void release ()
    {
        if (page_pool)
            page_pool->msgUnref (page_list.first);

        page_pool = NULL;
        page_list.reset ();
    }

    virtual Size toString_ (Memory const &mem,
                            Format const &fmt) const;

    MediaMessage () : msg_type (Type_None) {}

    MediaMessage (Type const msg_type)
        : msg_type          (msg_type),
          pts_nanosec       (0),
          dts_nanosec       (0),
          page_pool         (NULL),
          msg_len           (0),
          msg_offset        (0)
    {}

  // TODO Why don't we call release() in Message *virtual* dtor?
  //          - because we transfer pages ownership when sending messages?
};

// Must be copyable.
class AudioMessage : public MediaMessage
{
  public:
    AudioFrameType frame_type;
    AudioCodecId   codec_id;
    unsigned       rate;
    unsigned       channels;

    mt_iface (MediaMessage)
      Size toString_ (Memory const &mem,
                      Format const &fmt) const;
    mt_iface_end

    AudioMessage ()
        : MediaMessage (MediaMessage::Type_Audio),
          frame_type   (AudioFrameType::Unknown),
          codec_id     (AudioCodecId::Unknown),
          rate         (44100),
          channels     (1)
    {}
};

// Must be copyable.
class VideoMessage : public MediaMessage
{
  public:
    // Note that we ignore AVC composition time for now.

    VideoFrameType frame_type;
    VideoCodecId   codec_id;

    mt_iface (MediaMessage)
      Size toString_ (Memory const &mem,
                      Format const &fmt) const;
    mt_iface_end

    VideoMessage ()
        : MediaMessage (MediaMessage::Type_Video),
          frame_type   (VideoFrameType::Unknown),
          codec_id     (VideoCodecId::Unknown)
    {}
};

class MediaDesc_Audio;
class MediaDesc_Video;

class MediaDescMessage : public MediaMessage
{
  public:
    Ref<MediaDesc_Audio> media_desc__audio;
    Ref<MediaDesc_Video> media_desc__video;

    mt_iface (MediaMessage)
      Size toString_ (Memory const &mem,
                      Format const &fmt) const;
    mt_iface_end

    MediaDescMessage ()
        : MediaMessage (MediaMessage::Type_MediaDesc)
    {}
};

}


#include <moment/media_desc.h>


#endif /* MOMENT__MEDIA_MESSAGE__H__ */

