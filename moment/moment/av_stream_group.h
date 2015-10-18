/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__AV_STREAM_GROUP__H__
#define MOMENT__AV_STREAM_GROUP__H__


#include <moment/stream.h>


namespace Moment {

// TODO Rename to StreamGroup
class AvStreamGroup : public Object
{
private:
    StateMutex mutex;

public:
    struct Events
    {
        void (*closed) (void *cb_data);

        void (*numWatchersChanged) (Count  num_watchers,
                                    void  *cb_data);
    };

private:
    class StreamList_name;

    class StreamEntry : public IntrusiveListElement< StreamList_name >
    {
    public:
        mt_const AvStreamGroup *av_stream_group;

        mt_const Ref<VideoStream> stream;
        mt_mutex (mutex) Count num_watchers;
    };

    typedef IntrusiveList< StreamEntry, StreamList_name, DeleteAction<StreamEntry> > StreamList;

    mt_const StreamList stream_list;
    mt_const Count num_streams;

    mt_mutex (mutex) Count num_watchers;

    DeferredProcessor::Task deferred_num_watchers_changed_task;
    DeferredProcessor::Registration deferred_reg;


  // ______________________________ event_informer _____________________________

    Informer_<Events> event_informer;

    static void informClosed (Events * const events,
                              void   * const cb_data,
                              void   * const /* inform_data */)
    {
        if (events)
            events->closed (cb_data);
    }

    void fireClosed () { event_informer.informAll (informClosed, NULL /* inform_data */); }

    struct NumWatchersChanged_InformData
    {
        Count num_watchers;
    };

    static void informNumWatchersChanged (Events * const events,
                                          void   * const cb_data,
                                          void   * const _data)
    {
        if (events) {
            NumWatchersChanged_InformData * const data = static_cast <NumWatchersChanged_InformData*> (_data);
            events->numWatchersChanged (data->num_watchers, cb_data);
        }
    }

    void fireNumWatchersChanged (Count const tmp_num_watchers)
    {
        NumWatchersChanged_InformData data;
        data.num_watchers = tmp_num_watchers;
        event_informer.informAll (informNumWatchersChanged, &data);
    }

public:
    Informer_<Events>* getEventInformer () { return &event_informer; }

  // ___________________________________________________________________________


private:
    static VideoStream::StateEvents stream_state_events;

    static void streamNumWatchersChanged (Count  num_watchers,
                                          void  *_stream_entry);

public:
    mt_const void addStream (VideoStream * mt_nonnull stream);

    Count getNumStreams () const { return num_streams; }

    Ref<VideoStream> getPrimaryStream () const { return getStream (0); }

    VideoStream* getStream (Count idx) const;

    void start ();

    void close (bool close_streams);

    mt_const void init (DeferredProcessor * mt_nonnull deferred_processor);

     AvStreamGroup (EmbedContainer *embed_container);
    ~AvStreamGroup ();
};

typedef AvStreamGroup StreamGroup;
typedef AvStreamGroup MediaStreamGroup;

}


#endif /* MOMENT__AV_STREAM_GROUP__H__ */

