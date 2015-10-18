/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__SERVER_APP__H__
#define LIBMARY__SERVER_APP__H__


#include <libmary/types.h>
#include <libmary/exception.h>
#include <libmary/timers.h>
#include <libmary/active_poll_group.h>
#include <libmary/deferred_processor.h>
#include <libmary/deferred_connection_sender.h>
#include <libmary/server_context.h>

#ifdef LIBMARY_MT_SAFE
  #include <libmary/multi_thread.h>
#endif


namespace M {

class ServerApp : public ServerContext
{
private:
    StateMutex mutex;

public:
    struct Events
    {
        void (*threadStarted) (void *cb_data);
    };

private:
  #ifdef LIBMARY_MT_SAFE
    class ThreadData : public Object
    {
      public:
	Embed<Timers>                        timers;
	Embed<DefaultPollGroup>              poll_group;
	Embed<DeferredProcessor>             deferred_processor;
	Embed<DeferredConnectionSenderQueue> dcs_queue;

	Embed<ServerThreadContext> thread_ctx;

	ThreadData (EmbedContainer * const embed_container)
            : Object             (embed_container),
              timers             (this /* embed_container */),
              poll_group         (this /* embed_container */),
              deferred_processor (this /* embed_container */),
	      dcs_queue          (this /* embed_container */),
	      thread_ctx         (this /* embed_container */)
	{}
    };
  #endif

    Informer_<Events> event_informer;

    Embed<Timers>                        timers;
    Embed<DefaultPollGroup>              poll_group;
    Embed<DeferredProcessor>             deferred_processor;
    Embed<DeferredConnectionSenderQueue> dcs_queue;

    Embed<ServerThreadContext> main_thread_ctx;

    mt_const LibMary_ThreadLocal *init_tlocal;

  #ifdef LIBMARY_MT_SAFE
    mt_const Count num_threads;

    mt_const Ref<MultiThread> multi_thread;

    typedef List< Ref<ThreadData> > ThreadDataList;
    mt_mutex (mutex) ThreadDataList spawn_tdata_list;
    mt_mutex (mutex) ThreadDataList thread_data_list;
    mt_mutex (mutex) ThreadDataList::Element *thread_selector;
  #endif

    AtomicInt should_stop;

    static void informThreadStarted (Events *events,
                                     void   *cb_data,
                                     void   *inform_data);

    void fireThreadStarted ();

    static void firstTimerAdded (void *_active_poll_group);

    static void threadFunc (void *_self);

  mt_iface (ActivePollGroup::Frontend)
    static ActivePollGroup::Frontend poll_frontend;
    static void pollIterationBegin (void *_thread_ctx);
    static bool pollIterationEnd   (void *_thread_ctx);
  mt_iface_end

public:
  mt_iface (ServerContext)
    Ref<ServerThreadContext> selectThreadContext  ();
    Ref<ServerThreadContext> getMainThreadContext ();
  mt_iface_end

    Informer_<Events>* getEventInformer () { return &event_informer; }

    ServerContext* getServerContext () { return this; }

    mt_const mt_throws Result init (Count num_threads);

    mt_throws Result run ();

    void stop ();

    void release ();

    ServerApp (EmbedContainer *embed_container,
	       Count           num_threads = 0);

    ~ServerApp ();
};

}


#endif /* LIBMARY__SERVER_APP__H__ */

