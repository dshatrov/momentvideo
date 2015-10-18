/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/media_desc.h>


namespace Moment {

static void encodeMediaDesc_Audio (MediaDesc_Audio        * const mt_nonnull media_desc__audio,
                                   PagePool               * const mt_nonnull page_pool,
                                   PagePool::PageListHead * const mt_nonnull ret_page_list,
                                   Size                   * const mt_nonnull ret_len)
{
  // TODO getFillPages_locked for efficiency

    {
        Byte const hdr [] = { 1 };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 1;
    }

    {
        Byte const hdr [] = { (Byte) media_desc__audio->audio_codec_id };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 1;
    }

    {
        Byte const hdr [] = {
            (Byte) (media_desc__audio->rate >> 24),
            (Byte) (media_desc__audio->rate >> 16),
            (Byte) (media_desc__audio->rate >>  8),
            (Byte) (media_desc__audio->rate >>  0)
        };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 4;
    }

    {
        Byte const hdr [] = {
            (Byte) (media_desc__audio->channels >> 24),
            (Byte) (media_desc__audio->channels >> 16),
            (Byte) (media_desc__audio->channels >>  8),
            (Byte) (media_desc__audio->channels >>  0)
        };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 4;
    }

    switch (media_desc__audio->getType()) {
        case MediaDesc_Audio::Unknown:
        case MediaDesc_Audio::Known:
            break;
        case MediaDesc_Audio::AAC: {
            MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (media_desc__audio);

            {
                Byte const len = media_desc__aac->getCodecData().len();

                Byte const hdr [] = {
                    (Byte) (len >> 24),
                    (Byte) (len >> 16),
                    (Byte) (len >>  8),
                    (Byte) (len >>  0)
                };
                page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
                *ret_len += 4;

                page_pool->getFillPages (ret_page_list, media_desc__aac->getCodecData());
                *ret_len += len;
            }
        } break;
        case MediaDesc_Audio::Speex: {
            MediaDesc_Speex * const media_desc__speex = static_cast <MediaDesc_Speex*> (media_desc__audio);

            for (unsigned i = 0; i < 2; ++i) {
                Byte const len = media_desc__speex->getCodecData (i).len();

                Byte const hdr [] = {
                    (Byte) (len >> 24),
                    (Byte) (len >> 16),
                    (Byte) (len >>  8),
                    (Byte) (len >>  0)
                };
                page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
                *ret_len += 4;

                page_pool->getFillPages (ret_page_list, media_desc__speex->getCodecData (i));
                *ret_len += len;
            }
        } break;
    }
}

static void encodeMediaDesc_Video (MediaDesc_Video        * const mt_nonnull media_desc__video,
                                   PagePool               * const mt_nonnull page_pool,
                                   PagePool::PageListHead * const mt_nonnull ret_page_list,
                                   Size                   * const mt_nonnull ret_len)
{
    {
        Byte const hdr [] = { 2 };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 1;
    }

    {
        Byte const hdr [] = { (Byte) media_desc__video->video_codec_id };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 1;
    }

    {
        Byte const hdr [] = {
            (Byte) (media_desc__video->width >> 24),
            (Byte) (media_desc__video->width >> 16),
            (Byte) (media_desc__video->width >>  8),
            (Byte) (media_desc__video->width >>  0)
        };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 4;
    }

    {
        Byte const hdr [] = {
            (Byte) (media_desc__video->height >> 24),
            (Byte) (media_desc__video->height >> 16),
            (Byte) (media_desc__video->height >>  8),
            (Byte) (media_desc__video->height >>  0)
        };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 4;
    }

    {
        Byte const hdr [] = {
            (Byte) (media_desc__video->framerate >> 24),
            (Byte) (media_desc__video->framerate >> 16),
            (Byte) (media_desc__video->framerate >>  8),
            (Byte) (media_desc__video->framerate >>  0)
        };
        page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
        *ret_len += 4;
    }

    switch (media_desc__video->getType()) {
        case MediaDesc_Video::Unknown:
        case MediaDesc_Video::Known:
            break;
        case MediaDesc_Video::AVC: {
            MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (media_desc__video);

            {
                Byte const len = media_desc__avc->getCodecData().len();

                Byte const hdr [] = {
                    (Byte) (len >> 24),
                    (Byte) (len >> 16),
                    (Byte) (len >>  8),
                    (Byte) (len >>  0)
                };
                page_pool->getFillPages (ret_page_list, ConstMemory::forObject (hdr));
                *ret_len += 4;

                page_pool->getFillPages (ret_page_list, media_desc__avc->getCodecData());
                *ret_len += len;
            }
        } break;
    }
}

void encodeMediaDesc (MediaDesc              * const mt_nonnull media_desc,
                      PagePool               * const mt_nonnull page_pool,
                      PagePool::PageListHead * const mt_nonnull ret_page_list,
                      Size                   * const mt_nonnull ret_len)
{
    *ret_len = 0;

    switch (media_desc->getType()) {
        case MediaDesc::Audio:
            encodeMediaDesc_Audio (static_cast <MediaDesc_Audio*> (media_desc), page_pool, ret_page_list, ret_len);
            break;
        case MediaDesc::Video:
            encodeMediaDesc_Video (static_cast <MediaDesc_Video*> (media_desc), page_pool, ret_page_list, ret_len);
            break;
    }
}

static Ref<MediaDesc> decodeMediaDesc_Audio (PagePool::PageListArray * const mt_nonnull arr,
                                             Size pos,
                                             Size len)
{
    if (len < 9) {
        logD_ (_func, "media desc is too short: len ", len);
        return NULL;
    }

    AudioCodecId audio_codec_id;
    {
        Byte hdr [1];
        arr->get (pos, Memory::forObject (hdr));
        pos += 1;
        len -= 1;

        if (hdr [0] >= AudioCodecId::Last) {
            logD_ (_func, "invalid audio codec id: ", hdr [0]);
            return NULL;
        }

        audio_codec_id = (AudioCodecId::Value) hdr [0];
    }

    Uint32 rate;
    {
        Byte hdr [4];
        arr->get (pos, Memory::forObject (hdr));
        pos += 4;
        len -= 4;

        rate = ((Uint32) hdr [0] << 24) |
               ((Uint32) hdr [1] << 16) |
               ((Uint32) hdr [2] <<  8) |
               ((Uint32) hdr [3] <<  0);
    }

    Uint32 channels;
    {
        Byte hdr [4];
        arr->get (pos, Memory::forObject (hdr));
        pos += 4;
        len -= 4;

        channels = ((Uint32) hdr [0] << 24) |
                   ((Uint32) hdr [1] << 16) |
                   ((Uint32) hdr [2] <<  8) |
                   ((Uint32) hdr [3] <<  0);
    }

    Ref<MediaDesc_Audio> media_desc__audio;

    if (audio_codec_id == AudioCodecId::AAC) {
        if (len < 4) {
            logD_ (_func, "AAC media desc is too short: len ", len);
            return NULL;
        }

        Uint32 cdata_len;
        {
            Byte hdr [4];
            arr->get (pos, Memory::forObject (hdr));
            pos += 4;
            len -= 4;

            cdata_len = ((Uint32) hdr [0] << 24) |
                        ((Uint32) hdr [1] << 16) |
                        ((Uint32) hdr [2] <<  8) |
                        ((Uint32) hdr [3] <<  0);
        }

        if (len < cdata_len) {
            logD_ (_func, "AAC media desc is too short: len ", len, " cdata_len ", cdata_len);
            return NULL;
        }

        media_desc__audio = grab (new (std::nothrow) MediaDesc_AAC);
        MediaDesc_AAC * const media_desc__aac = static_cast <MediaDesc_AAC*> (media_desc__audio.ptr());

        media_desc__aac->allocateCodecData (cdata_len);
        arr->get (pos, media_desc__aac->getCodecData_rw ());
    } else
    if (audio_codec_id == AudioCodecId::Speex) {
        if (len < 8) {
            logD_ (_func, "Speex media desc is too short: len ", len);
            return NULL;
        }

        media_desc__audio = grab (new (std::nothrow) MediaDesc_Speex);
        MediaDesc_Speex * const media_desc__speex = static_cast <MediaDesc_Speex*> (media_desc__audio.ptr());

        for (unsigned i = 0; i < 2; ++i) {
            Uint32 cdata_len;

            Byte hdr [4];
            arr->get (pos, Memory::forObject (hdr));
            pos += 4;
            len -= 4;

            cdata_len = ((Uint32) hdr [0] << 24) |
                        ((Uint32) hdr [1] << 16) |
                        ((Uint32) hdr [2] <<  8) |
                        ((Uint32) hdr [3] <<  0);

            if (len < cdata_len) {
                logD_ (_func, "Speex media desc is too short: len ", len, " cdata_len ", cdata_len);
                return NULL;
            }

            media_desc__speex->allocateCodecData (i, cdata_len);
            arr->get (pos, media_desc__speex->getCodecData_rw (i));
        }
    } else {
        media_desc__audio = grab (new (std::nothrow) MediaDesc_Audio (
                audio_codec_id != AudioCodecId::Unknown ? MediaDesc_Audio::Known : MediaDesc_Audio::Unknown));
    }

    media_desc__audio->audio_codec_id = audio_codec_id;
    media_desc__audio->rate     = rate;
    media_desc__audio->channels = channels;

    return media_desc__audio;
}

static Ref<MediaDesc> decodeMediaDesc_Video (PagePool::PageListArray * const mt_nonnull arr,
                                             Size pos,
                                             Size len)
{
    if (len < 13) {
        logD_ (_func, "media desc is too short: len ", len);
        return NULL;
    }

    VideoCodecId video_codec_id;
    {
        Byte hdr [1];
        arr->get (pos, Memory::forObject (hdr));
        pos += 1;
        len -= 1;

        if (hdr [0] >= VideoCodecId::Last) {
            logD_ (_func, "invalid video codec id: ", hdr [0]);
            return NULL;
        }

        video_codec_id = (VideoCodecId::Value) hdr [0];
    }

    Uint32 width;
    {
        Byte hdr [4];
        arr->get (pos, Memory::forObject (hdr));
        pos += 4;
        len -= 4;

        width = ((Uint32) hdr [0] << 24) |
                ((Uint32) hdr [1] << 16) |
                ((Uint32) hdr [2] <<  8) |
                ((Uint32) hdr [3] <<  0);
    }

    Uint32 height;
    {
        Byte hdr [4];
        arr->get (pos, Memory::forObject (hdr));
        pos += 4;
        len -= 4;

        height = ((Uint32) hdr [0] << 24) |
                 ((Uint32) hdr [1] << 16) |
                 ((Uint32) hdr [2] <<  8) |
                 ((Uint32) hdr [3] <<  0);
    }

    Uint32 framerate;
    {
        Byte hdr [4];
        arr->get (pos, Memory::forObject (hdr));
        pos += 4;
        len -= 4;

        framerate = ((Uint32) hdr [0] << 24) |
                    ((Uint32) hdr [1] << 16) |
                    ((Uint32) hdr [2] <<  8) |
                    ((Uint32) hdr [3] <<  0);
    }

    Ref<MediaDesc_Video> media_desc__video;

    if (video_codec_id == VideoCodecId::AVC) {
        if (len < 4) {
            logD_ (_func, "AVC media desc is too short: len ", len);
            return NULL;
        }

        Uint32 cdata_len;
        {
            Byte hdr [4];
            arr->get (pos, Memory::forObject (hdr));
            pos += 4;
            len -= 4;

            cdata_len = ((Uint32) hdr [0] << 24) |
                        ((Uint32) hdr [1] << 16) |
                        ((Uint32) hdr [2] <<  8) |
                        ((Uint32) hdr [3] <<  0);
        }

        if (len < cdata_len) {
            logD_ (_func, "AVC media desc is too short: len ", len, " cdata_len ", cdata_len);
            return NULL;
        }

        media_desc__video = grab (new (std::nothrow) MediaDesc_AVC);
        MediaDesc_AVC * const media_desc__avc = static_cast <MediaDesc_AVC*> (media_desc__video.ptr());

        media_desc__avc->allocateCodecData (cdata_len);
        arr->get (pos, media_desc__avc->getCodecData_rw ());
    } else {
        media_desc__video = grab (new (std::nothrow) MediaDesc_Video (
                video_codec_id != VideoCodecId::Unknown ? MediaDesc_Video::Known : MediaDesc_Video::Unknown));
    }

    media_desc__video->video_codec_id = video_codec_id;
    media_desc__video->width     = width;
    media_desc__video->height    = height;
    media_desc__video->framerate = framerate;

    return media_desc__video;
}

Ref<MediaDesc> decodeMediaDesc (PagePool::Page * const page,
                                Size             const offs,
                                Size            len)
{
    if (len < 1) {
        logD_ (_func, "media desc is too short: len ", len);
        return NULL;
    }

    PagePool::PageListArray arr (page, offs, len);
    Size pos = 0;

    {
        Byte hdr [1];
        arr.get (pos, Memory::forObject (hdr));
        pos += 1;
        len -= 1;

        if (hdr [0] == 1) {
            return decodeMediaDesc_Audio (&arr, pos, len);
        } else
        if (hdr [0] == 2) {
            return decodeMediaDesc_Video (&arr, pos, len);
        } else {
            logD_ (_func, "invalid media desc type: ", hdr [0]);
            return NULL;
        }
    }

    unreachable ();
    return NULL;
}

}

