/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__STREAM_MANAGER__H__
#define MOMENT__STREAM_MANAGER__H__


#include <moment/av_stream_group.h>
#include <moment/config_manager.h>
#include <moment/event_service.h>


namespace Moment {

class StreamManager : public Object
{
  private:
    StateMutex mutex;

  public:
    struct StreamHandler
    {
        void (*streamAdded) (Stream      * mt_nonnull stream,
                             ConstMemory  stream_name,
                             void        *cb_data);

        void (*streamGroupAdded) (StreamGroup *stream_group,
                                  ConstMemory  stream_name,
                                  void        *cb_data);
    };

    class StreamHandlerKey
    {
        friend class StreamManager;
      private:
        GenericInformer::SubscriptionKey sbn_key;
      public:
        operator bool () const { return sbn_key; }
    };

    class StreamEntry;
    typedef List< Ref<StreamEntry> > StreamList;

    class StreamHashEntry : public StReferenced
    {
      public:
        StreamList stream_list;
    };

    typedef StringHash< StRef<StreamHashEntry> > StreamHash;

    class StreamEntry : public Object
    {
      public:
        Ref<Stream>          stream;
        StreamHash::EntryKey entry_key;

        mt_const StreamList::Element *list_el;

        mt_mutex (mutex) bool displaced;
        mt_mutex (mutex) bool valid;

        StreamEntry (EmbedContainer * const embed_container,
                     VideoStream    * const stream)
            : Object    (embed_container),
              stream    (stream),
              displaced (false),
              valid     (true)
        {}
    };

    class StreamKey
    {
        friend class StreamManager;
      private:
        WeakRef<StreamEntry> weak_stream_entry;
        StreamKey (StreamEntry * const stream_entry) : weak_stream_entry (stream_entry) {}
      public:
        operator bool () const { return weak_stream_entry.isValid(); }
        StreamKey () {}
    };

  private:
  public:
    // TODO make private
    class StreamInfo : public Referenced
    {
      public:
        // mt_mutex (mutex) Ref<RestreamInfo> restream_info;
        mt_mutex (mutex) bool waiting_for_streamer;

        // For restreamed streams, RestreamInfo::stream_key is used instead.
        mt_mutex (mutex) StreamKey stream_key;

        StreamInfo ()
            : waiting_for_streamer (false)
        {}
    };
  private:

    struct StreamAddedNotification
    {
        Ref<Stream>   stream;
        StRef<String> stream_name;
    };

    struct StreamClosedNotification
    {
        Ref<Stream> stream;
    };

    Informer_<StreamHandler> stream_informer;

    DeferredProcessor::Task stream_added_inform_task;
    DeferredProcessor::Task stream_closed_inform_task;
    DeferredProcessor::Registration deferred_reg;

    mt_const Ref<EventService> event_service;

    mt_mutex (mutex) List<StreamAddedNotification>  stream_added_notifications;
    mt_mutex (mutex) List<StreamClosedNotification> stream_closed_notifications;

    mt_const bool new_streams_on_top;
    mt_mutex (mutex) StreamHash stream_hash;

    static void informStreamAdded (StreamHandler *stream_handler,
                                   void          *cb_data,
                                   void          *inform_data);

    mt_mutex (mutex) void notifyDeferred_StreamAdded (Stream      * mt_nonnull stream,
                                                      ConstMemory  stream_name);

    mt_mutex (mutex) void notifyDeferred_StreamClosed (Stream * mt_nonnull stream);

    static bool streamAddedInformTask  (void *_self);
    static bool streamClosedInformTask (void *_self);

  public:
    mt_locks   (mutex) void lock   () { mutex.lock   (); }
    mt_unlocks (mutex) void unlock () { mutex.unlock (); }

    mt_mutex (mutex) StreamHandlerKey addStreamHandler_locked (CbDesc<StreamHandler> const &stream_handler);
                     StreamHandlerKey addStreamHandler        (CbDesc<StreamHandler> const &stream_handler);

    mt_mutex (mutex) void removeStreamHandler_locked (StreamHandlerKey handler_key);
                     void removeStreamHandler        (StreamHandlerKey handler_key);

    mt_mutex (mutex) StreamKey addStream_locked (Stream       *stream,
                                                 ConstMemory   path,
                                                 bool          fire_stream_added = true);
    StreamKey addStream (Stream      *stream,
                         ConstMemory  path,
                         bool         fire_stream_added = true);

    mt_mutex (mutex) void removeStream_locked (StreamKey const &stream_key);
                     void removeStream        (StreamKey const &stream_key);

    // TODO There's a logical problem here. A stream can only be deleted
    // by the one who created it. This limitation makes little sense.
    // But overcoming it requires more complex synchronization.
    mt_mutex (mutex) Ref<Stream> getStream_locked (ConstMemory stream_name);
                     Ref<Stream> getStream        (ConstMemory stream_name);

    mt_const Result init (ConfigManager     * mt_nonnull config_manager,
                          DeferredProcessor * mt_nonnull deferred_processor,
                          EventService      *event_service);

    StreamManager (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__STREAM_MANAGER__H__ */

