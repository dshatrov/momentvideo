/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__PUSH_AGENT__H__
#define MOMENT__PUSH_AGENT__H__


#include <libmary/libmary.h>
#include <moment/push_protocol.h>
#include <moment/stream_manager.h>


namespace Moment {

// PushAgent забирает видеопоток из Stream с определённым именем
// и проксит его в заданный приёмник. Первый приёмник - RTMP-сервер.
// Протоколы для push'инга регистрируются в хэше, который есть в MomentServer.
//
class PushAgent : public Object
{
private:
    mt_const StRef<String> stream_name;

    mt_const Ref<PushConnection> push_conn;
    mt_const Ref<Stream> bound_stream;

    void doStreamAdded (Stream * mt_nonnull stream);

  mt_iface (StreamManager::StreamHandler)
    static StreamManager::StreamHandler stream_handler;

    static void streamAdded (Stream      * mt_nonnull stream,
                             ConstMemory  stream_name,
                             void        *_self);
  mt_iface_end

public:
    mt_const void init (DeferredProcessor * mt_nonnull deferred_processor,
                        StreamManager     * mt_nonnull stream_manager,
                        ConstMemory        _stream_name,
                        PushProtocol      * mt_nonnull push_protocol,
                        ConstMemory        uri,
                        ConstMemory        username,
                        ConstMemory        password);

    PushAgent (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__PUSH_AGENT__H__ */

