/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__PUSH_PROTOCOL__H__
#define MOMENT__PUSH_PROTOCOL__H__


#include <libmary/libmary.h>

#include <moment/stream.h>


namespace Moment {

class PushConnection : public Object
{
  public:
    // Nothing here.

    PushConnection (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

class PushProtocol : public Object
{
  public:
    // 1. Connect (protocol-specific, +auth)
    // 2. Push messages: audio, video

    virtual Ref<PushConnection> connect (Stream      * mt_nonnull stream,
                                         ConstMemory  uri,
                                         ConstMemory  username,
                                         ConstMemory  password) = 0;

    PushProtocol (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__PUSH_PROTOCOL__H__ */

