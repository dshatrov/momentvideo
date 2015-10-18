/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__SOURCE_MANAGER__H__
#define MOMENT__SOURCE_MANAGER__H__


#include <moment/push_protocol.h>
#include <moment/fetch_protocol.h>
#include <moment/stream_source_provider.h>
#include <moment/stream_manager.h>


namespace Moment {

class SourceManager : public Object
{
  private:
    StateMutex mutex;

    mt_const WeakRef<StreamManager> weak_stream_manager;
    mt_const WeakRef<ServerContext> weak_server_ctx;


  // __________________________ Push/fetch protocols ___________________________

  private:
    typedef StringHash< Ref<PushProtocol> >  PushProtocolHash;
    typedef StringHash< Ref<FetchProtocol> > FetchProtocolHash;

    mt_mutex (mutex) PushProtocolHash  push_protocol_hash;
    mt_mutex (mutex) FetchProtocolHash fetch_protocol_hash;

  public:
    void addPushProtocol (ConstMemory   protocol_name,
                          PushProtocol * mt_nonnull push_protocol);

    void addFetchProtocol (ConstMemory    protocol_name,
                           FetchProtocol * mt_nonnull fetch_protocol);

    Ref<PushProtocol>  getPushProtocolForUri  (ConstMemory uri);
    Ref<FetchProtocol> getFetchProtocolForUri (ConstMemory uri);


  // _________________________ media source providers __________________________

  private:
    mt_const Ref<StreamSourceProvider> stream_source_provider;

  public:
    Ref<StreamSource> createStreamSource (CbDesc<StreamSource::Frontend> const &frontend,
                                          Timers            *timers,
                                          DeferredProcessor *deferred_processor,
                                          PagePool          *page_pool,
                                          AvStreamGroup     * mt_nonnull stream_group,
                                          Time               initial_seek,
                                          ChannelOptions    *channel_opts,
                                          PlaybackItem      *playback_item);

    mt_const void setStreamSourceProvider (StreamSourceProvider * const stream_source_provider)
        { this->stream_source_provider = stream_source_provider; }

  // ___________________________________________________________________________


    mt_const void init (StreamManager * mt_nonnull stream_manager,
                        ServerContext * mt_nonnull server_ctx);

    SourceManager (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__SOURCE_MANAGER__H__ */

