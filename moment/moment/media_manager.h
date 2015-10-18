/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MEDIA_MANAGER__H__
#define MOMENT__MEDIA_MANAGER__H__


#include <moment/stream_manager.h>


namespace Moment {

class MediaManager : public Object
{
  private:
    StateMutex mutex;

  public:
    struct MediaProvider
    {
        Ref<MediaSource> (*getMediaSource) (ConstMemory  path_tail,
                                            void        *cb_data);
    };

  private:
    struct MediaProviderEntry
    {
        Cb<MediaProvider> provider_cb;
    };

    mt_const Ref<StreamManager> stream_manager;

    mt_mutex (mutex) NamespaceContainer<MediaProviderEntry> media_providers;

  public:
    void addMediaProvider (ConstMemory const path,
                           CbDesc<MediaProvider> const &provider_cb);

    Ref<MediaSource> getMediaSource (ConstMemory path,
                                     bool        enable_vod);

    mt_const void init (StreamManager *stream_manager);

    MediaManager (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__MEDIA_MANAGER__H__ */

