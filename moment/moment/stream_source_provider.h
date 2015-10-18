/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__STREAM_SOURCE_PROVIDER__H__
#define MOMENT__STREAM_SOURCE_PROVIDER__H__


#include <moment/av_stream_group.h>
#include <moment/stream_source.h>
#include <moment/channel_options.h>


namespace Moment {

using namespace M;

class StreamSourceProvider : public Object
{
public:
    virtual Ref<StreamSource> createStreamSource (CbDesc<StreamSource::Frontend> const &frontend,
                                                  Timers            *timers,
                                                  DeferredProcessor *deferred_processor,
                                                  PagePool          *page_pool,
                                                  AvStreamGroup     *stream_group,
                                                  Time               initial_seek,
                                                  ChannelOptions    *channel_opts,
                                                  PlaybackItem      *playback_item) = 0;

    StreamSourceProvider (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__STREAM_SOURCE_PROVIDER__H__ */

