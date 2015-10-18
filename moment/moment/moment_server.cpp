/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/inc.h>

#include <moment/moment_server.h>


namespace Moment {

MOMENT__MOMENT_SERVER

// DEPRECATED MomentServer* MomentServer::instance = NULL;

mt_const Result
MomentServer::init (ServerApp        * const mt_nonnull server_app,
                    PagePool         * const mt_nonnull page_pool,
                    ServerThreadPool * const mt_nonnull recorder_thread_pool,
                    ServerThreadPool * const mt_nonnull reader_thread_pool,
                    Storage          * const mt_nonnull storage,
                    EventService     * const event_service,
                    MConfig::Config  * const mt_nonnull config)
{
    this->server_app           = server_app;
    this->page_pool            = page_pool;
    this->recorder_thread_pool = recorder_thread_pool;
    this->reader_thread_pool   = reader_thread_pool;
    this->storage              = storage;
    this->event_service        = event_service;

    config_manager->init (config);

    if (!http_manager->init (config_manager->getConfig(),
                             server_app->getServerContext(),
                             page_pool))
    {
        logE_ (_func, "http_manager.init() failed");
        return Result::Failure;
    }

    if (!stream_manager->init (config_manager,
                               server_app->getServerContext()->getMainThreadContext()->getDeferredProcessor(),
                               event_service))
    {
        logE_ (_func, "stream_manager.init() failed");
        return Result::Failure;
    }

    media_manager->init (stream_manager);

    source_manager->init (stream_manager,
                          server_app->getServerContext());

    channel_manager->init (this /* moment */, page_pool);

    client_manager->init (server_app->getServerContext()->getMainThreadContext()->getDeferredProcessor(),
                          stream_manager,
                          media_manager,
                          auth_manager,
                          config_manager);

    return Result::Success;
}

MomentServer::MomentServer (EmbedContainer * const embed_container)
    : Object               (embed_container),
      event_informer       (this /* outer_object */, &mutex),

      stream_manager       (this /* embed_container */),
      media_manager        (this /* embed_container */),
      source_manager       (this /* embed_container */),
      http_manager         (this /* embed_container */),
      page_request_manager (this /* embed_container */),
      config_manager       (this /* embed_container */),
      channel_manager      (this /* embed_container */),
      transcoding_manager  (this /* embed_container */),
      auth_manager         (this /* embed_container */),
      client_manager       (this /* embed_container */)
{
#if 0
// DEPRECATED
    assert (!instance);
    instance = this;
#endif
}

MomentServer::~MomentServer ()
{
    logD_ (_func_);

    fireDestroy ();
}

}

