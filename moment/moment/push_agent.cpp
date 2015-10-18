/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/push_agent.h>


using namespace M;

namespace Moment {

void
PushAgent::doStreamAdded (Stream * const stream)
{
    bound_stream->bindToStream (stream,
                                stream,
                                true /* bind_audio */,
                                true /* bind_video */);
}

StreamManager::StreamHandler PushAgent::stream_handler = {
    streamAdded,
    NULL /* streamGroupAdded */
};

void
PushAgent::streamAdded (Stream * const mt_nonnull stream,
                        ConstMemory   const stream_name,
                        void        * const _self)
{
    PushAgent * const self = static_cast <PushAgent*> (_self);

    if (!equal (stream_name, self->stream_name->mem()))
        return;

    self->doStreamAdded (stream);
}

mt_const void
PushAgent::init (DeferredProcessor * const mt_nonnull deferred_processor,
                 StreamManager     * const mt_nonnull stream_manager,
                 ConstMemory         const _stream_name,
                 PushProtocol      * const mt_nonnull push_protocol,
                 ConstMemory         const uri,
                 ConstMemory         const username,
                 ConstMemory         const password)
{
    stream_name = newString (_stream_name);

    bound_stream = grabNewObject <MediaStream> (deferred_processor);

    {
      // Getting video stream by name, bind the stream, subscribe for video_stream_handler *atomically*.
        stream_manager->lock ();

        stream_manager->addStreamHandler_locked (
                CbDesc<StreamManager::StreamHandler> (&stream_handler, this, this));

        Ref<Stream> const stream = stream_manager->getStream_locked (_stream_name);
        if (stream)
            doStreamAdded (stream);

        stream_manager->unlock ();
    }

    push_conn = push_protocol->connect (bound_stream, uri, username, password);
}

}

