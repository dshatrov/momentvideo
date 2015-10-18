/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__NVR_VOD_SERVICE__H__
#define MOMENT_NVR__NVR_VOD_SERVICE__H__


#include <moment-nvr/nvr_reader.h>


namespace MomentNvr {

class NvrVodService : public Object
{
  private:
    mt_const Ref<PagePool>         page_pool;
    mt_const Ref<ServerThreadPool> reader_thread_pool;
    mt_const StRef<Vfs>            vfs;
    mt_const StRef<String>         root_path;

    static void readerSourceDeleted (void *_data);

    mt_const void initConfig (MomentServer * mt_nonnull moment);

  public:
    mt_iface (MediaManager::MediaProvider)
      static MediaManager::MediaProvider const media_provider;

      static Ref<MediaSource> getMediaSource (ConstMemory  path,
                                              void        *_self);
    mt_iface_end

    mt_const void init (MomentServer * mt_nonnull moment,
                        Vfs          * mt_nonnull vfs,
                        ConstMemory  root_path);

    NvrVodService (EmbedContainer *embed_container);
};

}


#endif /* MOMENT_NVR__NVR_VOD_SERVICE__H__ */

