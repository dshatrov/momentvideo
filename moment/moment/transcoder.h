/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__TRANSCODER__H__
#define MOMENT__TRANSCODER__H__


#include <libmary/libmary.h>

#include <moment/stream.h>


namespace Moment {

using namespace M;

class Transcoder : public Object
{
public:
    enum TranscodingMode
    {
        TranscodingMode_Off,
        TranscodingMode_On,
        TranscodingMode_Direct
    };

    mt_const virtual void addOutputStream (VideoStream     * mt_nonnull out_stream,
                                           ConstMemory      chain_str,
                                           TranscodingMode  audio_mode,
                                           TranscodingMode  video_mode) = 0;

    // Must be called only once, *after* init() and addOutputSrteam() are called.
    virtual void start (VideoStream * mt_nonnull src_stream) = 0;

    mt_const virtual void init (DeferredProcessor * mt_nonnull deferred_processor,
                                Timers            * mt_nonnull timers,
                                PagePool          * mt_nonnull page_pool,
                                bool               transcode_on_demand,
                                Time               transcode_on_demand_timeout_millisec) = 0;

    Transcoder (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__TRANSCODER__H__ */

