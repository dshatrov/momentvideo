/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__MEDIA_DESC__H__
#define LIBMOMENT__MEDIA_DESC__H__


#include <libmary/libmary.h>

#include <moment/media_message.h>


namespace Moment {

// TODO Make MediaDesc StReferenced. Implement a clone() virtual method.
//
class MediaDesc : public Referenced
{
  public:
    enum Type {
        Audio,
        Video
    };

    static ConstMemory typeString (Type const type)
    {
        switch (type) {
            case Audio:
                return ConstMemory ("Audio");
            case Video:
                return ConstMemory ("Video");
        }

        return ConstMemory();
    }

  private:
    Type const type;

  public:
    Type getType() const { return type; }

    virtual bool equals (MediaDesc * const mt_nonnull right) { return type == right->type; }

    virtual Size toString_ (Memory const &mem, Format const & /* fmt */) const
        { return printToString (mem, typeString (type)); }

    MediaDesc (Type const type)
        : type (type)
    {}
};

class MediaDesc_Audio : public MediaDesc
{
  public:
    enum Type {
        Unknown,
        Known,
        AAC,
        Speex
    };

    static ConstMemory typeString (Type const type)
    {
        switch (type) {
            case Unknown:
                return ConstMemory ("Unknown");
            case Known:
                return ConstMemory ("Known");
            case AAC:
                return ConstMemory ("AAC");
            case Speex:
                return ConstMemory ("Speex");
        }

        return ConstMemory();
    }

  protected:
    Type const type;

  public:
    AudioCodecId audio_codec_id;

    mt_const Uint32 rate;
    mt_const Uint32 channels;

    Type getType() const { return type; }

    virtual bool equals (MediaDesc * const mt_nonnull right)
    {
        if (!MediaDesc::equals (right))
            return false;

        MediaDesc_Audio * const right_audio = static_cast <MediaDesc_Audio*> (right);
        if (   type           == right_audio->type
            && audio_codec_id == right_audio->audio_codec_id
            && rate           == right_audio->rate
            && channels       == right_audio->channels)
        {
            return true;
        }

        return false;
    }

    virtual Size toString_ (Memory const &mem, Format const &fmt) const
    {
        Size const size = MediaDesc::toString_ (mem, fmt);
        return size + printToString (mem.safeRegion (size), " ",
                typeString (type), " "
                "codec ",    audio_codec_id, " "
                "rate ",     rate, " "
                "channels ", channels);
    }

    mt_const void init (Uint32 const rate,
                        Uint32 const channels)
    {
        this->rate = rate;
        this->channels = channels;
    }

    MediaDesc_Audio (AudioCodecId const codec_id)
        : MediaDesc      (Audio),
          type           (Known),
          audio_codec_id (codec_id),
          rate           (0),
          channels       (0)
    {}

    MediaDesc_Audio (Type const type = Unknown)
        : MediaDesc      (Audio),
          type           (type),
          audio_codec_id (AudioCodecId::Unknown),
          rate           (0),
          channels       (0)
    {}
};

class MediaDesc_Video : public MediaDesc
{
  public:
    enum Type {
        Unknown,
        Known,
        AVC
    };

    static ConstMemory typeString (Type const type)
    {
        switch (type) {
            case Unknown:
                return ConstMemory ("Unknown");
            case Known:
                return ConstMemory ("Known");
            case AVC:
                return ConstMemory ("AVC");
        }

        return ConstMemory();
    }

  private:
    Type const type;

  public:
    VideoCodecId video_codec_id;

    mt_const Uint32 width;
    mt_const Uint32 height;
    mt_const Uint32 framerate;

    Type getType() const { return type; }

    virtual bool equals (MediaDesc * const mt_nonnull right)
    {
        if (!MediaDesc::equals (right))
            return false;

        MediaDesc_Video * const right_video = static_cast <MediaDesc_Video*> (right);
        if (   type      == right_video->type
            && width     == right_video->width
            && height    == right_video->height
            && framerate == right_video->framerate)
        {
            return true;
        }

        return false;
    }

    virtual Size toString_ (Memory const &mem, Format const &fmt) const
    {
        Size const size = MediaDesc::toString_ (mem, fmt);
        return size + printToString (mem.safeRegion (size), " ",
                typeString (type), " "
                "codec ",     video_codec_id, " "
                "width ",     width, " "
                "height ",    height, " "
                "framerate ", framerate);
    }

    mt_const void init (Uint32 const width,
                        Uint32 const height,
                        Uint32 const framerate)
    {
        this->width = width;
        this->height = height;
        this->framerate = framerate;
    }

    MediaDesc_Video (VideoCodecId const codec_id)
        : MediaDesc      (Video),
          type           (Known),
          video_codec_id (codec_id),
          width          (0),
          height         (0),
          framerate      (0)
    {}

    MediaDesc_Video (Type const type = Unknown)
        : MediaDesc      (Video),
          type           (type),
          video_codec_id (VideoCodecId::Unknown),
          width          (0),
          height         (0),
          framerate      (0)
    {}
};

class MediaDesc_Speex : public MediaDesc_Audio
{
  protected:
    mt_const Buffer codec_data_buffers [2];

  public:
    Memory getCodecData_rw (unsigned const idx) const
    {
        assert (idx < 2);
        return codec_data_buffers [idx].mem;
    }

    ConstMemory getCodecData (unsigned const idx) const
    {
        return getCodecData_rw (idx);
    }

    void allocateCodecData (unsigned const idx,
                            Size     const len)
    {
        assert (idx < 2);
        codec_data_buffers [idx].allocate (len);
    }

    void setCodecData (ConstMemory const codec_data,
                       unsigned    const idx)
    {
        allocateCodecData (idx, codec_data.len());
        if (codec_data.len())
            memcpy (codec_data_buffers [idx].mem.buf(), codec_data.buf(), codec_data.len());
    }

    virtual bool equals (MediaDesc * const mt_nonnull _right)
    {
        if (!MediaDesc_Audio::equals (_right))
            return false;

        MediaDesc_Speex * const right = static_cast <MediaDesc_Speex*> (_right);
        if (   equal (codec_data_buffers [0].mem, right->codec_data_buffers [0].mem)
            && equal (codec_data_buffers [1].mem, right->codec_data_buffers [1].mem))
        {
            return true;
        }

        return false;
    }

    virtual Size toString_ (Memory const &mem, Format const &fmt) const
    {
        Size const size = MediaDesc_Audio::toString_ (mem, fmt);
        return size + printToString (mem.safeRegion (size), " ",
                "codec_data[0] ", fmt_hex_mem, codec_data_buffers[0].mem,
                "codec_data[1] ", fmt_hex_mem, codec_data_buffers[1].mem);
    }

    MediaDesc_Speex ()
        : MediaDesc_Audio (Speex)
    {
        audio_codec_id = AudioCodecId::Speex;
    }
};

class MediaDesc_AAC : public MediaDesc_Audio
{
  protected:
    mt_const Buffer codec_data_buffer;

  public:
    Memory getCodecData_rw () const { return codec_data_buffer.mem; }

    ConstMemory getCodecData () const { return getCodecData_rw(); }

    void allocateCodecData (Size const len)
    {
        codec_data_buffer.allocate (len);
    }

    void setCodecData (ConstMemory const codec_data)
    {
        allocateCodecData (codec_data.len());
        if (codec_data.len())
            memcpy (codec_data_buffer.mem.buf(), codec_data.buf(), codec_data.len());
    }

    virtual bool equals (MediaDesc * const mt_nonnull _right)
    {
        if (!MediaDesc_Audio::equals (_right))
            return false;

        MediaDesc_AAC * const right = static_cast <MediaDesc_AAC*> (_right);
        if (equal (codec_data_buffer.mem, right->codec_data_buffer.mem))
            return true;

        return false;
    }

    virtual Size toString_ (Memory const &mem, Format const &fmt) const
    {
        Size const size = MediaDesc_Audio::toString_ (mem, fmt);
        return size + printToString (mem.safeRegion (size), " ",
                "codec_data ", fmt_hex_mem, codec_data_buffer.mem);
    }

    mt_const void init (/* Uint32      const depth, */
                        Uint32      const rate,
                        Uint32      const channels,
                        ConstMemory const codec_data)
    {
        MediaDesc_Audio::init (/* depth, */ rate, channels);
        setCodecData (codec_data);
    }

    MediaDesc_AAC ()
        : MediaDesc_Audio (AAC)
    {
        audio_codec_id = AudioCodecId::AAC;
    }
};

class MediaDesc_AVC : public MediaDesc_Video
{
  protected:
    mt_const Buffer codec_data_buffer;

  public:
    Memory getCodecData_rw () const { return codec_data_buffer.mem; }

    ConstMemory getCodecData () const { return getCodecData_rw(); }

    void allocateCodecData (Size const len)
    {
        codec_data_buffer.allocate (len);
    }

    void setCodecData (ConstMemory const codec_data)
    {
        allocateCodecData (codec_data.len());
        if (codec_data.len())
            memcpy (codec_data_buffer.mem.buf(), codec_data.buf(), codec_data.len());
    }

    virtual bool equals (MediaDesc * const mt_nonnull _right)
    {
        if (!MediaDesc_Video::equals (_right))
            return false;

        MediaDesc_AVC * const right = static_cast <MediaDesc_AVC*> (_right);
        if (equal (codec_data_buffer.mem, right->codec_data_buffer.mem))
            return true;

        return false;
    }

    virtual Size toString_ (Memory const &mem, Format const &fmt) const
    {
        Size const size = MediaDesc_Video::toString_ (mem, fmt);
        return size + printToString (mem.safeRegion (size), " ",
                "codec_data ", fmt_hex_mem, codec_data_buffer.mem);
    }

    mt_const void init (Uint32      const width,
                        Uint32      const height,
                        Uint32      const framerate,
                        ConstMemory const codec_data)
    {
        MediaDesc_Video::init (width, height, framerate);

        codec_data_buffer.allocate (codec_data.len());
        if (codec_data.len())
            memcpy (codec_data_buffer.mem.buf(), codec_data.buf(), codec_data.len());
    }

    MediaDesc_AVC ()
        : MediaDesc_Video (AVC)
    {
        video_codec_id = VideoCodecId::AVC;
    }
};

void encodeMediaDesc (MediaDesc              * mt_nonnull media_desc,
                      PagePool               * mt_nonnull page_pool,
                      PagePool::PageListHead * mt_nonnull ret_page_list,
                      Size                   * mt_nonnull ret_len);

Ref<MediaDesc> decodeMediaDesc (PagePool::Page *page,
                                Size            offs,
                                Size            len);

}


#endif /* LIBMOMENT__MEDIA_DESC__H__ */

