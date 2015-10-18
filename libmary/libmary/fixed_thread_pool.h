/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__FIXED_THREAD_POOL__H__
#define LIBMARY__FIXED_THREAD_POOL__H__


#include <libmary/types.h>
#include <libmary/atomic.h>
#include <libmary/server_thread_pool.h>
#include <libmary/timers.h>
#include <libmary/active_poll_group.h>
#include <libmary/deferred_processor.h>
#include <libmary/deferred_connection_sender.h>
#ifdef LIBMARY_MT_SAFE
  #include <libmary/multi_thread.h>
#endif


namespace M {

class FixedThreadPool : public ServerThreadPool
{
private:
    StateMutex mutex;

  #ifdef LIBMARY_MT_SAFE
    class ThreadData : public Object
    {
    public:
        Embed<ServerThreadContext>           thread_ctx;
        Embed<Timers>                        timers;
        Embed<DefaultPollGroup>              poll_group;
        Embed<DeferredProcessor>             deferred_processor;
        Embed<DeferredConnectionSenderQueue> dcs_queue;

        ThreadData (EmbedContainer * const embed_container)
            : Object             (embed_container),
              thread_ctx         (/*embed_container=*/ this),
              timers             (/*embed_container=*/ this),
              poll_group         (/*embed_container=*/ this),
              deferred_processor (/*embed_container=*/ this),
              dcs_queue          (/*embed_container=*/ this)
        {}
    };
  #endif

    mt_const Ref<ServerThreadContext> main_thread_ctx;

  #ifdef LIBMARY_MT_SAFE
    mt_const Embed<MultiThread> multi_thread;

    typedef List< Ref<ThreadData> > ThreadDataList;
    mt_mutex (mutex) ThreadDataList spawn_tdata_list;
// FIXME Nothing is put into thread_data_list, ever! - I've tried to fixed that, but not sure in code correctness now
    // TODO Could be mt_const
    mt_mutex (mutex) ThreadDataList thread_data_list;
    mt_mutex (mutex) ThreadDataList::Element *thread_selector;

    AtomicInt should_stop;

    static void firstTimerAdded (void *_thread_ctx);

    mt_iface (DeferredProcessor::Backend)
        static DeferredProcessor::Backend const deferred_processor_backend;

        static void deferred_processor_trigger (void *_thread_data);
    mt_iface_end

    static void threadFunc (void *_self);

    mt_iface (ActivePollGroup::Frontend)
      static ActivePollGroup::Frontend const poll_frontend;

      static void pollIterationBegin (void *_thread_data);

      static bool pollIterationEnd (void *_thread_data);
    mt_iface_end
  #endif

public:
  mt_iface (ServerThreadPool)
    mt_throws Ref<ServerThreadContext> grabThreadContext (ConstMemory  filename,
                                                          Object      *guard_obj = NULL);

    void releaseThreadContext (ServerThreadContext *thread_ctx);
  mt_iface_end

    mt_throws Result spawn ();

    void stop ();

    // Should be called before spawn().
    mt_const Result init (Count                num_threads,
                          ServerThreadContext * mt_nonnull main_thread_ctx);

    FixedThreadPool (EmbedContainer *embed_container,
                     Count           num_threads = 0);
};

}


#endif /* LIBMARY__FIXED_THREAD_POOL__H__ */

