/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__STORAGE__H__
#define MOMENT__STORAGE__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class Storage : public Object
{
  public:
    class StorageFile : public Object
    {
      public:
        virtual Connection* getConnection () = 0;

        StorageFile (EmbedContainer * const embed_container) : Object (embed_container) {}
    };

    virtual mt_throws Ref<StorageFile> openFile (ConstMemory        filename,
                                                 DeferredProcessor * mt_nonnull deferred_processor,
                                                 Uint32             create_permissions = LIBMARY__DEFAULT_CREATE_PERMISSIONS) = 0;

    Storage (EmbedContainer * const embed_container) : Object (embed_container) {}
};

}


#endif /* MOMENT__STORAGE__H__ */

