/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/nvr_vod_service.h>


namespace MomentNvr {

namespace {
    struct SourceDeletedData : public Referenced
    {
        NvrVodService            *nvr_vod_service;
        Ref<ServerThreadContext>  thread_ctx;
    };
}

void
NvrVodService::readerSourceDeleted (void * const _data)
{
    SourceDeletedData * const data = static_cast <SourceDeletedData*> (_data);
    NvrVodService * const self = data->nvr_vod_service;

    self->reader_thread_pool->releaseThreadContext (data->thread_ctx);
}

MediaManager::MediaProvider const NvrVodService::media_provider = {
    getMediaSource
};

Ref<MediaSource>
NvrVodService::getMediaSource (ConstMemory  path,
                               void        * const _self)
{
    NvrVodService * const self = static_cast <NvrVodService*> (_self);

    logD_ (_self_func "path: \"", path, "\"");
    path = stripSlashes (path);

    StRef<String> const full_path = makeString (self->root_path, "/", path);

    StRef<NvrReader> const nvr_reader = st_grabNew <NvrReader> ();
    nvr_reader->init (self->page_pool, self->vfs, path);

    Ref<ReaderVodSource> const reader_vod_source = grabNewObject <ReaderVodSource> ();

    Ref<ServerThreadContext> const thread_ctx = self->reader_thread_pool->grabThreadContext (full_path);
    {
        Ref<SourceDeletedData> const data = grabNew <SourceDeletedData> ();
        data->nvr_vod_service = self;
        data->thread_ctx = thread_ctx;

        reader_vod_source->addDeletionCallback (
                CbDesc<Object::DeletionCallback> (readerSourceDeleted, data, self, data));
    }

    reader_vod_source->init (thread_ctx->getDeferredProcessor(),
                             thread_ctx->getTimers(),
                             nvr_reader);
    return reader_vod_source;
}

mt_const void
NvrVodService::initConfig (MomentServer * const mt_nonnull moment)
{
    Ref<MConfig::Config> const config = moment->getConfigManager()->getConfig();

  // No-op. No config options yet.
}

mt_const void
NvrVodService::init (MomentServer * const mt_nonnull moment,
                     Vfs          * const mt_nonnull vfs,
                     ConstMemory    const root_path)
{
    page_pool          = moment->getPagePool();
    reader_thread_pool = moment->getReaderThreadPool();
    this->vfs          = vfs;
    this->root_path    = newString (root_path);

    initConfig (moment);

    moment->getMediaManager()->addMediaProvider (
            "nvr",
            CbDesc<MediaManager::MediaProvider> (&media_provider, this, this));
}

NvrVodService::NvrVodService (EmbedContainer * const embed_container)
    : Object (embed_container)
{
}

}

