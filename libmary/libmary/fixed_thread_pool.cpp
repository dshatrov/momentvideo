/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <libmary/log.h>


#include <libmary/fixed_thread_pool.h>


namespace M {

#ifdef LIBMARY_MT_SAFE
void
FixedThreadPool::firstTimerAdded (void * const _thread_data)
{
    ThreadData * const thread_data = static_cast <ThreadData*> (_thread_data);
    thread_data->poll_group->trigger ();
}

DeferredProcessor::Backend const FixedThreadPool::deferred_processor_backend = {
    deferred_processor_trigger
};

void
FixedThreadPool::deferred_processor_trigger (void * const _thread_data)
{
    ThreadData * const thread_data = static_cast <ThreadData*> (_thread_data);
    thread_data->poll_group->trigger ();
}

void
FixedThreadPool::threadFunc (void * const _self)
{
    FixedThreadPool * const self = static_cast <FixedThreadPool*> (_self);

    if (!updateTime ())
        logE_ (_func, "updateTime() failed: ", exc->toString());

    self->mutex.lock ();
    assert (!self->spawn_tdata_list.isEmpty());
    Ref<ThreadData> const thread_data = self->spawn_tdata_list.getFirst();
    self->spawn_tdata_list.remove (self->spawn_tdata_list.getFirstElement());
    self->mutex.unlock ();

    thread_data->poll_group->bindToThread (libMary_getThreadLocal());

    // Note that things should work without this extra check as well.
    // If they don't, then fix it.
    if (self->should_stop.get ())
        return;

    for (;;) {
        if (!thread_data->poll_group->poll (thread_data->timers->getSleepTime_microseconds())) {
            logE_ (_self_func, "poll_group.poll() failed: ", exc->toString());
            // TODO This is a fatal error, but we should exit gracefully nonetheless.
            abort ();
            break;
        }

        if (self->should_stop.get())
            break;
    }
}

ActivePollGroup::Frontend const FixedThreadPool::poll_frontend = {
    pollIterationBegin,
    pollIterationEnd
};

void
FixedThreadPool::pollIterationBegin (void * const _thread_data)
{
    ThreadData * const thread_data = static_cast <ThreadData*> (_thread_data);

    if (!updateTime ())
        logE_ (_func, "updateTime() failed: ", exc->toString());

    thread_data->timers->processTimers ();
}

bool
FixedThreadPool::pollIterationEnd (void * const _thread_data)
{
    ThreadData * const thread_data = static_cast <ThreadData*> (_thread_data);
    return thread_data->deferred_processor->process ();
}
#endif // LIBMARY_MT_SAFE

mt_throws Ref<ServerThreadContext>
FixedThreadPool::grabThreadContext (ConstMemory   const /* filename */,
                                    Object      * const /* guard_obj */
                                                        /* ^ ignored since releaseThreadContext() is a no-op */)
{
  #ifdef LIBMARY_MT_SAFE
    mutex.lock ();

    ServerThreadContext *thread_ctx;
    if (thread_selector) {
        thread_ctx = thread_selector->data->thread_ctx;
        thread_selector = thread_selector->next;
    } else {
        if (!thread_data_list.isEmpty()) {
            thread_ctx = thread_data_list.getFirst()->thread_ctx;
            thread_selector = thread_data_list.getFirstElement()->next;
        } else {
            thread_ctx = main_thread_ctx;
        }
    }

    mutex.unlock ();

    return thread_ctx;
  #else
    return main_thread_ctx.ptr();
  #endif // LIBMARY_MT_SAFE
}

void
FixedThreadPool::releaseThreadContext (ServerThreadContext * const /* thread_ctx */)
{
  // No-op
}

mt_throws Result
FixedThreadPool::spawn ()
{
  #ifdef LIBMARY_MT_SAFE
    if (!multi_thread->spawn (false /* joinable */)) {
        logE_ (_this_func, "multi_thread->spawn() failed: ", exc->toString());
        return Result::Failure;
    }
  #endif

    return Result::Success;
}

void
FixedThreadPool::stop ()
{
  #ifdef LIBMARY_MT_SAFE
    should_stop.set (1);

    mutex.lock ();

    {
        ThreadDataList::iter iter (thread_data_list);
        while (!thread_data_list.iter_done (iter)) {
            ThreadData * const thread_data = thread_data_list.iter_next (iter)->data;
            if (!thread_data->poll_group->trigger ())
                logE_ (_this_func, "poll_group.trigger() failed: ", exc->toString());
        }
    }

    mutex.unlock ();
  #endif
}

mt_const Result
FixedThreadPool::init (Count                 const num_threads,
                       ServerThreadContext * const mt_nonnull main_thread_ctx)
{
    this->main_thread_ctx = main_thread_ctx;

  #ifdef LIBMARY_MT_SAFE
    multi_thread->setNumThreads (num_threads);

    for (Count i = 0; i < num_threads; ++i) {
        Ref<ThreadData> const thread_data = grabNewObject <ThreadData> ();

        thread_data->dcs_queue->setDeferredProcessor (thread_data->deferred_processor);

        thread_data->thread_ctx->init (thread_data->timers,
                                       thread_data->poll_group,
                                       thread_data->deferred_processor,
                                       thread_data->dcs_queue);

        if (!thread_data->poll_group->open ()) {
            logE_ (_this_func, "poll_group.open() failed: ", exc->toString());
            return Result::Failure;
        }

        thread_data->poll_group->setFrontend (
                CbDesc<ActivePollGroup::Frontend> (&poll_frontend, thread_data, thread_data));

        thread_data->timers->setFirstTimerAddedCallback (
                CbDesc<Timers::FirstTimerAddedCallback> (firstTimerAdded, thread_data, thread_data));

        thread_data->deferred_processor->setBackend (
                CbDesc<DeferredProcessor::Backend> (&deferred_processor_backend, thread_data, thread_data));

        spawn_tdata_list.append (thread_data);
        thread_data_list.append (thread_data);
    }
  #else
    (void) num_threads;
  #endif

    return Result::Success;
}

FixedThreadPool::FixedThreadPool (EmbedContainer * const embed_container,
                                  Count            const num_threads)
    : ServerThreadPool (embed_container)
  #ifdef LIBMARY_MT_SAFE
      , multi_thread (/*embed_container=*/ this,
                      num_threads,
                      CbDesc<Thread::ThreadFunc> (threadFunc, this, this))

      , thread_selector (NULL)
  #endif
{
  #ifndef LIBMARY_MT_SAFE
    (void) num_threads;
  #endif
}

}

