/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__LOCAL_STORAGE__H__
#define MOMENT__LOCAL_STORAGE__H__


#include <moment/storage.h>


namespace Moment {

class LocalStorage : public Storage
{
  private:
    class LocalStorageFile : public Storage::StorageFile
    {
      public:
        Embed<NativeFile>     file;
        Embed<FileConnection> conn;

        mt_iface (Storage::StorageFile)
          Connection* getConnection () { return conn; }
        mt_iface_end

        LocalStorageFile (EmbedContainer * const embed_container)
            : StorageFile (embed_container),
              file        (this /* embed_container */),
              conn        (this /* embed_container */)
        {}
    };

    mt_const StRef<Vfs> vfs;

  public:
    mt_iface (Storage)
      mt_throws Ref<StorageFile> openFile (ConstMemory        filename,
                                           DeferredProcessor * mt_nonnull deferred_processor,
                                           Uint32             create_permissions = LIBMARY__DEFAULT_CREATE_PERMISSIONS);
    mt_iface_end

    LocalStorage (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__LOCAL_STORAGE__H__ */

