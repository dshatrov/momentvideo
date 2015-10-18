/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__SLAVE_STREAM_SOURCE__H__
#define MOMENT__SLAVE_STREAM_SOURCE__H__


#include <moment/stream_source.h>
#include <moment/stream_manager.h>


namespace Moment {

using namespace M;

class SlaveStreamSource : public StreamSource
{
private:
    mt_const StRef<String> master_stream_name;
    mt_const Cb<StreamSource::Frontend> frontend;
    mt_const Ref<Stream> bind_stream;

  mt_iface (StreamManager::StreamHandler)
    static StreamManager::StreamHandler const stream_handler;

    static void streamAdded (Stream      * mt_nonnull stream,
                             ConstMemory  stream_name,
                             void        *_self);
  mt_iface_end

public:
  mt_iface (StreamSource)
    void releasePipeline () {}

    void getTrafficStats (TrafficStats * const mt_nonnull ret_traffic_stats) { ret_traffic_stats->reset(); }
    void resetTrafficStats () {}
  mt_iface_end

    void init (StreamManager * mt_nonnull stream_manager,
               ConstMemory    stream_name,
               Stream        * mt_nonnull bind_stream,
               CbDesc<StreamSource::Frontend> const &frontend);

    SlaveStreamSource (EmbedContainer *embed_container)
        : StreamSource (embed_container)
    {}
};

}


#endif /* MOMENT__SLAVE_STREAM_SOURCE__H__ */

