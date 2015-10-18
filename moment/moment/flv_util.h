/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__FLV_UTIL__H__
#define LIBMOMENT__FLV_UTIL__H__


#include <moment/stream.h>


namespace Moment {

enum {
    FlvAudioHeader_MaxLen = 2,
    FlvVideoHeader_MaxLen = 5
};

unsigned fillFlvAudioHeader (AudioMessage * mt_nonnull audio_msg,
                             Memory mem);

unsigned fillFlvVideoHeader (VideoMessage * mt_nonnull video_msg,
                             Memory mem);

static inline unsigned flvSamplingRateToNumeric (Byte const flv_rate)
{
    switch (flv_rate) {
        case 0:
            return 5512;
        case 1:
            return 11025;
        case 2:
            return 22050;
        case 3:
            return 44100;
    }

    return 44100;
}

static inline unsigned numericSamplingRateToFlv (unsigned const rate)
{
    switch (rate) {
        case 5512:
        case 5513:
            return 0;
        case 8000:
        case 11025:
        case 16000:
            return 1;
        case 22050:
            return 2;
        case 44100:
            return 3;
    }

    return 3;
}

}


#endif /* LIBMOMENT__FLV_UTIL__H__ */

