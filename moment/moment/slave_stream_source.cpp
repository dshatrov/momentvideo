/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/slave_stream_source.h>


namespace Moment {

StreamManager::StreamHandler const SlaveStreamSource::stream_handler = {
    streamAdded,
    NULL /* streamGroupAdded */
};

void
SlaveStreamSource::streamAdded (Stream      * const mt_nonnull stream,
                                ConstMemory   const stream_name,
                                void        * const _self)
{
    SlaveStreamSource * const self = static_cast <SlaveStreamSource*> (_self);

    if (!equal (stream_name, self->master_stream_name->mem()))
        return;

    if (self->frontend)
        self->frontend.call (self->frontend->gotVideo);

    self->bind_stream->bindToStream (stream, stream, true, true);
}

void
SlaveStreamSource::init (StreamManager * const mt_nonnull stream_manager,
                         ConstMemory     const stream_name,
                         Stream        * const mt_nonnull bind_stream,
                         CbDesc<StreamSource::Frontend> const &frontend)
{
    this->master_stream_name = st_grab (new (std::nothrow) String (stream_name));
    this->bind_stream = bind_stream;
    this->frontend = frontend;

    stream_manager->lock ();

    if (Stream * const stream = stream_manager->getStream_locked (stream_name))
        bind_stream->bindToStream (stream, stream, true, true);

    stream_manager->addStreamHandler_locked (
            CbDesc<StreamManager::StreamHandler> (&stream_handler, this, this));

    stream_manager->unlock ();
}

}

