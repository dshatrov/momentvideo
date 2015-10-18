/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/media_manager.h>


namespace Moment {

void
MediaManager::addMediaProvider (ConstMemory const path,
                                CbDesc<MediaProvider> const &provider_cb)
{
    mutex.lock ();
    {
        NamespaceContainer<MediaProviderEntry>::Entry * const entry = media_providers.addEntry (path);
        MediaProviderEntry * const provider_entry = &entry->data;
        provider_entry->provider_cb = provider_cb;
    }
    mutex.unlock ();
}

Ref<MediaSource>
MediaManager::getMediaSource (ConstMemory const path,
                              bool        const enable_vod)
{
    logD_ (_this_func, "path \"", path, "\", enable_vod ", enable_vod);

    mutex.lock ();

    ConstMemory path_tail;
    NamespaceContainer<MediaProviderEntry>::Entry *entry = NULL;
    if (enable_vod) {
        logD_ (_func, "calling mediaProviders.getEntry(), path \"", path, "\"");
        entry = media_providers.getEntry (path, &path_tail);
        if (entry)
            logD_ (_func, "got entry, path_tail \"", path_tail, "\"");
        else
            logD_ (_func, "no media provider for path \"", path, "\"");
    }

    if (!entry) {
        if (stream_manager) {
            mutex.unlock ();

            // Stripping the first path component if present (e.g. "live").
            ConstMemory stream_name = path;
            if (stream_name.len()) {
                if (Byte const * const slash_pos = (Byte const *) memchr (stream_name.buf(), '/', stream_name.len()))
                    stream_name = stream_name.region (slash_pos - stream_name.buf() + 1 /* '/' */);
            }

            Ref<MediaSource> media_source = stream_manager->getStream (stream_name);
            if (!media_source && !equal (stream_name, path))
                media_source = stream_manager->getStream (path);

            return media_source;
        }
    }
    MediaProviderEntry* const media_provider_entry = &entry->data;

    Cb<MediaProvider> const provider_cb = media_provider_entry->provider_cb;
    mutex.unlock ();

    Ref<MediaSource> media_source;
    if (!provider_cb.call_ret (&media_source,
                               provider_cb->getMediaSource,
                               /*(*/
                                   path_tail
                               /*)*/)) {
        logD_ (_this_func, "media provider gone");
        return NULL;
    }

    return media_source;
}

mt_const void
MediaManager::init (StreamManager * const stream_manager)
{
    this->stream_manager = stream_manager;
}

MediaManager::MediaManager (EmbedContainer * const embed_container)
    : Object (embed_container)
{}

}

