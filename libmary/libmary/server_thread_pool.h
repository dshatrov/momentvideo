/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__SERVER_THREAD_POOL__H__
#define LIBMARY__SERVER_THREAD_POOL__H__


#include <libmary/server_context.h>


namespace M {

class ServerThreadPool : public Object
{
public:
    virtual mt_throws Ref<ServerThreadContext> grabThreadContext (ConstMemory  filename,
                                                                  Object      *guard_obj = NULL) = 0;

    virtual void releaseThreadContext (ServerThreadContext *thread_ctx) = 0;

    ServerThreadPool (EmbedContainer * const embed_container) : Object (embed_container) {}
};

}


#endif /* LIBMARY__SERVER_THREAD_POOL__H__ */

