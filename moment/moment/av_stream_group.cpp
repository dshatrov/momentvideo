/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/av_stream_group.h>


namespace Moment {

VideoStream::StateEvents AvStreamGroup::stream_state_events = {
    NULL /* closed */,
    streamNumWatchersChanged,
    NULL /* publishingStatusChanged */,
    NULL /* mediaDesc */
};

void
AvStreamGroup::streamNumWatchersChanged (Count   const num_watchers,
                                         void  * const _stream_entry)
{
    StreamEntry * const stream_entry = static_cast <StreamEntry*> (_stream_entry);
    AvStreamGroup * const self = stream_entry->av_stream_group;

    self->mutex.lock ();
    assert (self->num_watchers >= stream_entry->num_watchers);
    self->num_watchers = self->num_watchers - stream_entry->num_watchers + num_watchers;
    stream_entry->num_watchers = num_watchers;
    self->mutex.unlock ();

    // TODO fireNumWatchersChanged()_locked (synchronization - ?)

//#warning This is wrong because of lack of synchronization.
//#warning A convenient message delivery mechanism is badly needed.
}

mt_const void
AvStreamGroup::addStream (VideoStream * const mt_nonnull stream)
{
    StreamEntry * const stream_entry = new (std::nothrow) StreamEntry;
    stream_entry->av_stream_group = this;
    stream_entry->stream = stream;
    stream_entry->num_watchers = 0;

    stream_list.append (stream_entry);
    ++num_streams;
}

VideoStream*
AvStreamGroup::getStream (Count const idx) const
{
    StreamEntry *stream_entry = NULL;

    StreamList::iterator iter (stream_list);
    for (Count i = 0; i <= idx; ++i) {
        if (iter.done())
            return NULL;

        stream_entry = iter.next ();
    }
    assert (stream_entry);

    return stream_entry->stream.ptr();
}

void
AvStreamGroup::start ()
{
    logD_ (_this_func_);

    mutex.lock ();
    {
        StreamList::iterator iter (stream_list);
        while (!iter.done()) {
            logD_ (_this_func, "iteration");

            StreamEntry * const stream_entry = iter.next ();

            stream_entry->stream->lock ();

            num_watchers += stream_entry->stream->getNumWatchers_locked();

            stream_entry->stream->getStateEventInformer()->subscribe_locked (
                    CbDesc<VideoStream::StateEvents> (&stream_state_events, stream_entry, this));

            stream_entry->stream->unlock ();
        }
    }
    mutex.unlock ();
}

void
AvStreamGroup::close (bool const /* close_streams */)
{
  // Note that we don't care to unsubscribe from streams' state events
  // because close() is supposed to occur near the end of AvStreamGroup's
  // lifetime.

    fireClosed ();
}

mt_const void
AvStreamGroup::init (DeferredProcessor * const mt_nonnull deferred_processor)
{
    deferred_reg.setDeferredProcessor (deferred_processor);
}

AvStreamGroup::AvStreamGroup (EmbedContainer * const embed_container)
    : Object         (embed_container),
      num_streams    (0),
      num_watchers   (0),
      event_informer (this, &mutex)
{
}

AvStreamGroup::~AvStreamGroup ()
{
    fireClosed ();
}

}

