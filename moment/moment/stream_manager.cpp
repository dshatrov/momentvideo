/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_config.h>

#include <moment/stream_manager.h>


namespace Moment {

namespace {
    class InformStreamAdded_Data
    {
    public:
        Stream      * const stream;
        ConstMemory   const stream_name;

        InformStreamAdded_Data (Stream      * const stream,
                                ConstMemory   const stream_name)
            : stream      (stream),
              stream_name (stream_name)
        {}
    };
}

void
StreamManager::informStreamAdded (StreamHandler * const stream_handler,
                                  void          * const cb_data,
                                  void          * const _inform_data)
{
    if (stream_handler->streamAdded) {
        InformStreamAdded_Data * const inform_data =
                static_cast <InformStreamAdded_Data*> (_inform_data);
        stream_handler->streamAdded (inform_data->stream,
                                     inform_data->stream_name,
                                     cb_data);
    }
}

mt_mutex (mutex) void
StreamManager::notifyDeferred_StreamAdded (Stream      * const mt_nonnull stream,
                                           ConstMemory   const stream_name)
{
    logD_ (_this_func, "stream 0x", fmt_hex, (UintPtr) stream);

    StreamAddedNotification * const notification = &stream_added_notifications.appendEmpty()->data;
    notification->stream = stream;
    notification->stream_name = newString (stream_name);

    deferred_reg.scheduleTask (&stream_added_inform_task, false /* permanent */);
}

mt_mutex (mutex) void
StreamManager::notifyDeferred_StreamClosed (Stream * const mt_nonnull stream)
{
    StreamClosedNotification * const notification = &stream_closed_notifications.appendEmpty()->data;
    notification->stream = stream;

    deferred_reg.scheduleTask (&stream_closed_inform_task, false /* permanent */);
}

bool
StreamManager::streamAddedInformTask (void * const _self)
{
    StreamManager * const self = static_cast <StreamManager*> (_self);

    logD_ (_self_func_);

    self->mutex.lock ();
    while (!self->stream_added_notifications.isEmpty()) {
        StreamAddedNotification * const notification = &self->stream_added_notifications.getFirst();

        Ref<Stream> stream;
        stream.setNoRef ((Stream*) notification->stream);
        notification->stream.setNoUnref ((Stream*) NULL);

        StRef<String> stream_name;
        stream_name.setNoRef ((String*) notification->stream_name);
        notification->stream_name.setNoUnref ((String*) NULL);

        self->stream_added_notifications.remove (self->stream_added_notifications.getFirstElement());

        InformStreamAdded_Data inform_data (stream, String::mem (stream_name));
        logD_ (_self_func, "informing streamAdded, stream_name ", stream_name);
        mt_unlocks_locks (mutex) self->stream_informer.informAll_locked (informStreamAdded, &inform_data);
    }
    self->mutex.unlock ();

    return false /* Do not reschedule */;
}

bool
StreamManager::streamClosedInformTask (void * const _self)
{
    StreamManager * const self = static_cast <StreamManager*> (_self);

    self->mutex.lock ();
    while (!self->stream_closed_notifications.isEmpty()) {
        StreamClosedNotification * const notification = &self->stream_closed_notifications.getFirst();

        Ref<Stream> stream;
        stream.setNoRef ((VideoStream*) notification->stream);
        notification->stream.setNoUnref ((VideoStream*) NULL);

        self->stream_closed_notifications.remove (self->stream_closed_notifications.getFirstElement());
        self->mutex.unlock ();

        stream->close ();

        self->mutex.lock ();
    }
    self->mutex.unlock ();

    return false /* Do not reschedule */;
}

mt_mutex (mutex) StreamManager::StreamHandlerKey
StreamManager::addStreamHandler_locked (CbDesc<StreamHandler> const &stream_handler)
{
    StreamHandlerKey stream_handler_key;
    stream_handler_key.sbn_key = stream_informer.subscribe_locked (stream_handler);
    return stream_handler_key;
}

StreamManager::StreamHandlerKey
StreamManager::addStreamHandler (CbDesc<StreamHandler> const &stream_handler)
{
    StreamHandlerKey stream_handler_key;
    stream_handler_key.sbn_key = stream_informer.subscribe (stream_handler);
    return stream_handler_key;
}

mt_mutex (mutex) void
StreamManager::removeStreamHandler_locked (StreamHandlerKey const stream_handler_key)
{
    stream_informer.unsubscribe_locked (stream_handler_key.sbn_key);
}

void
StreamManager::removeStreamHandler (StreamHandlerKey const stream_handler_key)
{
    stream_informer.unsubscribe (stream_handler_key.sbn_key);
}

mt_mutex (mutex) StreamManager::StreamKey
StreamManager::addStream_locked (Stream      * const stream,
                                 ConstMemory   const path,
                                 bool          const fire_stream_added)
{
    logD_ (_func, "name: ", path, ", stream 0x", fmt_hex, (UintPtr) stream);

    StreamEntry * const stream_entry = new (std::nothrow) StreamEntry (NULL /* embed_container */, stream);
    assert (stream_entry);

    {
        StreamHash::EntryKey const entry_key = stream_hash.lookup (path);
        if (entry_key) {
            stream_entry->entry_key = entry_key;

            StRef<StreamHashEntry> * const hash_entry = entry_key.getDataPtr();
            assert (!(*hash_entry)->stream_list.isEmpty());

            bool stream_added = false;

            Stream * const old_stream = (*hash_entry)->stream_list.getFirst()->stream;
            Stream::MomentServerData * const old_stream_data = &old_stream->moment_data;
            if (old_stream_data->stream_info) {
                StreamInfo * const old_stream_info = static_cast <StreamInfo*> (old_stream_data->stream_info.ptr());

              // If the previous stream is waiting for streamer,
              // then bind the previous stream to the new one.

                logD_ (_func, "binding old_stream 0x", fmt_hex, (UintPtr) old_stream);

                if (old_stream_info->waiting_for_streamer) {
                    old_stream_info->waiting_for_streamer = false;

                    logD_ (_func, "waiting_for_streamer complete");

                    old_stream->setPublishingInProgress (true);
                    old_stream->bindToStream (stream /* bind_audio_stream */,
                                              stream /* bind_video_stream */,
                                              true   /* bind_audio */,
                                              true   /* bind_video */,
                                              true   /* propagate_close_audio */,
                                              true   /* propagate_close_video */);

                    stream_entry->list_el = (*hash_entry)->stream_list.prepend (stream_entry);
                    stream_added = true;
                }
            }

            if (!stream_added) {
                if (new_streams_on_top) {
                    (*hash_entry)->stream_list.getFirst()->displaced = true;
                    notifyDeferred_StreamClosed ((*hash_entry)->stream_list.getFirst()->stream);
                    stream_entry->list_el = (*hash_entry)->stream_list.prepend (stream_entry);
                } else {
                    stream_entry->list_el = (*hash_entry)->stream_list.append (stream_entry);
                }
            }
        } else {
            StRef<StreamHashEntry> const hash_entry = st_grab (new (std::nothrow) StreamHashEntry);
            stream_entry->list_el = hash_entry->stream_list.append (stream_entry);

            stream_entry->entry_key = stream_hash.add (path, hash_entry);
        }
    }

    if (fire_stream_added) {
        notifyDeferred_StreamAdded (stream, path);

        if (event_service) {
            StRef<String> esc_path;
            event_service->sendEvent (makeString ("stream add ", lineEscape (path, &esc_path)));
        }
    }

    return StreamKey (stream_entry);
}

StreamManager::StreamKey
StreamManager::addStream (Stream      * const stream,
                          ConstMemory   const path,
                          bool          const fire_stream_added)
{
    mutex.lock ();
    StreamKey const stream_key = addStream_locked (stream, path, fire_stream_added);
    mutex.unlock ();
    return stream_key;
}

mt_mutex (mutex) void
StreamManager::removeStream_locked (StreamKey const &stream_key)
{
    Ref<StreamEntry> const stream_entry = stream_key.weak_stream_entry.getRef ();
    if (!stream_entry)
        return;

    if (!stream_entry->valid) {
        return;
    }
    stream_entry->valid = false;

    logD_ (_func, "name: ", stream_entry->entry_key.getKey(), ", "
           "stream 0x", fmt_hex, (UintPtr) stream_entry->stream.ptr());

    StreamHash::EntryKey const hash_key = stream_entry->entry_key;
    StreamHashEntry * const hash_entry = *hash_key.getDataPtr();
    hash_entry->stream_list.remove (stream_entry->list_el);
    if (hash_entry->stream_list.isEmpty()) {
        logD_ (_func, "last stream ", hash_key.getKey());
        stream_hash.remove (hash_key);
    }
}

void
StreamManager::removeStream (StreamKey const &stream_key)
{
    mutex.lock ();
    removeStream_locked (stream_key);
    mutex.unlock ();
}

Ref<Stream>
StreamManager::getStream (ConstMemory const path)
{
    mutex.lock ();
    Ref<Stream> const stream = getStream_locked (path);
    mutex.unlock ();
    return stream;
}

mt_mutex (mutex) Ref<Stream>
StreamManager::getStream_locked (ConstMemory const path)
{
    StreamHash::EntryKey const entry_key = stream_hash.lookup (path);
    if (!entry_key)
	return NULL;

    StreamEntry * const stream_entry = (*entry_key.getDataPtr())->stream_list.getFirst();
    assert (stream_entry);

    if (stream_entry->displaced)
        return NULL;

    return stream_entry->stream;
}

mt_const Result
StreamManager::init (ConfigManager     * const mt_nonnull config_manager,
                     DeferredProcessor * const mt_nonnull deferred_processor,
                     EventService      * const event_service)
{
    this->event_service = event_service;
    deferred_reg.setDeferredProcessor (deferred_processor);

    {
        Ref<MConfig::Config> const config = config_manager->getConfig ();

        if (!configGetBoolean (config, "moment/new_streams_on_top", &new_streams_on_top, new_streams_on_top, _func))
            return Result::Failure;
    }

    return Result::Success;
}

StreamManager::StreamManager (EmbedContainer * const embed_container)
    : Object             (embed_container),
      stream_informer    (this /* outer_object */, &mutex),
      new_streams_on_top (true)
{
    stream_added_inform_task.cb =
            CbDesc<DeferredProcessor::TaskCallback> (streamAddedInformTask, this, this);
    stream_closed_inform_task.cb =
            CbDesc<DeferredProcessor::TaskCallback> (streamClosedInformTask, this, this);
}

}

