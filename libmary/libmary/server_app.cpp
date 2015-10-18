/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cstdio>

#include <libmary/deferred_connection_sender.h>
#include <libmary/util_time.h>
#include <libmary/log.h>

#include <libmary/server_app.h>


namespace M {

static LogGroup libMary_logGroup_server_app ("server_app", LogLevel::I);

Ref<ServerThreadContext>
ServerApp::selectThreadContext ()
{
  #ifdef LIBMARY_MT_SAFE
    ServerThreadContext *thread_ctx;
    {
        mutex.lock ();

        if (thread_selector) {
            thread_ctx      = thread_selector->data->thread_ctx;
            thread_selector = thread_selector->next;
        } else {
            if (!thread_data_list.isEmpty()) {
                thread_ctx      = thread_data_list.getFirst()->thread_ctx;
                thread_selector = thread_data_list.getFirstElement()->next;
            } else {
                thread_ctx = main_thread_ctx;
            }
        }

        mutex.unlock ();
    }

    return thread_ctx;
  #else
    return main_thread_ctx.ptr();
  #endif // LIBMARY_MT_SAFE
}

Ref<ServerThreadContext>
ServerApp::getMainThreadContext ()
{
    return main_thread_ctx.ptr();
}

void
ServerApp::informThreadStarted (Events * const events,
                                void   * const cb_data,
                                void   * const /* inform_data */)
{
    if (events->threadStarted)
        events->threadStarted (cb_data);
}

void
ServerApp::fireThreadStarted ()
{
    event_informer.informAll (informThreadStarted, NULL /* inform_cb_data */);
}

void
ServerApp::firstTimerAdded (void * const _active_poll_group)
{
    logD (server_app, _func_);
    ActivePollGroup * const active_poll_group = static_cast <ActivePollGroup*> (_active_poll_group);
    active_poll_group->trigger ();
}

ActivePollGroup::Frontend ServerApp::poll_frontend = {
    pollIterationBegin,
    pollIterationEnd
};

void
ServerApp::pollIterationBegin (void * const _thread_ctx)
{
    ServerThreadContext * const thread_ctx = static_cast <ServerThreadContext*> (_thread_ctx);

    if (!updateTime ())
        logE_ (_func, "updateTime() failed: ", exc->toString());

    thread_ctx->getTimers()->processTimers ();
}

bool
ServerApp::pollIterationEnd (void * const _thread_ctx)
{
    ServerThreadContext * const thread_ctx = static_cast <ServerThreadContext*> (_thread_ctx);
    thread_ctx->getDeletionContext()->process ();
    return thread_ctx->getDeferredProcessor()->process ();
}

static void deferred_processor_trigger (void * const _active_poll_group)
{
    ActivePollGroup * const active_poll_group = static_cast <ActivePollGroup*> (_active_poll_group);
    active_poll_group->trigger ();
}

static DeferredProcessor::Backend deferred_processor_backend = {
    deferred_processor_trigger
};

mt_const mt_throws Result
ServerApp::init (Count const param__num_threads)
{
  #ifdef LIBMARY_MT_SAFE
    this->num_threads = param__num_threads;
  #endif

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    init_tlocal = tlocal;

    if (!poll_group->open ())
        return Result::Failure;

    dcs_queue->setDeferredProcessor (deferred_processor);

    main_thread_ctx->init (timers,
                          poll_group,
                          deferred_processor,
                          dcs_queue);
    tlocal->deletion_ctx = main_thread_ctx->getDeletionContext();

    poll_group->setFrontend (
            CbDesc<ActivePollGroup::Frontend> (
                    &poll_frontend,
                    main_thread_ctx,
                    main_thread_ctx));

    timers->setFirstTimerAddedCallback (
            CbDesc<Timers::FirstTimerAddedCallback> (
                    firstTimerAdded,
                    static_cast <ActivePollGroup*> (poll_group),
                    poll_group));

    deferred_processor->setBackend (
            CbDesc<DeferredProcessor::Backend> (
                    &deferred_processor_backend,
                    static_cast <ActivePollGroup*> (poll_group),
                    poll_group));

  #ifdef LIBMARY_MT_SAFE
    for (Count i = 0; i < num_threads; ++i) {
        Ref<ThreadData> const thread_data = grabNewObject <ThreadData> ();

        thread_data->dcs_queue->setDeferredProcessor (thread_data->deferred_processor);

        thread_data->thread_ctx->init (thread_data->timers,
                                       thread_data->poll_group,
                                       thread_data->deferred_processor,
                                       thread_data->dcs_queue);

        // Deprecated thread_data->poll_group->bindToThread (tlocal);
        if (!thread_data->poll_group->open ()) {
            logE_ (_func, "poll_group.open() failed: ", exc->toString());
            return Result::Failure;
        }

        thread_data->poll_group->setFrontend (
                CbDesc<ActivePollGroup::Frontend> (
                        &poll_frontend,
                        thread_data->thread_ctx,
                        thread_data->thread_ctx));

        thread_data->timers->setFirstTimerAddedCallback (
                CbDesc<Timers::FirstTimerAddedCallback> (
                        &firstTimerAdded,
                        static_cast <ActivePollGroup*> (thread_data->poll_group.ptr()),
                        thread_data->poll_group));

        thread_data->deferred_processor->setBackend (
                CbDesc<DeferredProcessor::Backend> (
                        &deferred_processor_backend,
                        static_cast <ActivePollGroup*> (thread_data->poll_group.ptr()),
                        thread_data->poll_group));

        spawn_tdata_list.append (thread_data);
        thread_data_list.append (thread_data);
    }

    multi_thread->setNumThreads (num_threads);
  #endif

    return Result::Success;
}

#ifdef LIBMARY_MT_SAFE
void
ServerApp::threadFunc (void * const _self)
{
    ServerApp * const self = static_cast <ServerApp*> (_self);

    if (!updateTime ())
        logE_ (_func, "updateTime() failed: ", exc->toString());

    self->mutex.lock ();

    assert (!self->spawn_tdata_list.isEmpty());
    Ref<ThreadData> const thread_data = self->spawn_tdata_list.getFirst();
    self->spawn_tdata_list.remove (self->spawn_tdata_list.getFirstElement());

    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    tlocal->deletion_ctx = thread_data->thread_ctx->getDeletionContext();

    if (self->should_stop.get()) {
        self->mutex.unlock ();
        goto _return;
    }

    self->mutex.unlock ();

    self->fireThreadStarted ();

    for (;;) {
        if (!thread_data->poll_group->poll (thread_data->timers->getSleepTime_microseconds())) {
            logE_ (_func, "poll_group.poll() failed: ", exc->toString());
            // TODO This is a fatal error, but we should exit gracefully nevertheless.
            abort ();
            break;
        }

        if (self->should_stop.get())
            break;
    }

  _return:
    thread_data->dcs_queue->release ();
}
#endif // LIBMARY_MT_SAFE

mt_throws Result
ServerApp::run ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    // init() must be called from the same thread as run() (for deletion_ctx).
    assert (tlocal == init_tlocal);

    poll_group->bindToThread (tlocal);

  #ifdef LIBMARY_MT_SAFE
    if (!multi_thread->spawn (true /* joinable */)) {
        logE_ (_func, "multi_thread->spawn() failed: ", exc->toString());
        return Result::Failure;
    }
  #endif

    for (;;) {
        if (should_stop.get())
            break;

        logD (server_app, _func, "iteration");
        if (!poll_group->poll (timers->getSleepTime_microseconds())) {
            logE_ (_func, "poll_group.poll() failed: ", exc->toString());
            stop ();
          #ifdef LIBMARY_MT_SAFE
            multi_thread->join ();
          #endif
            return Result::Failure;
        }
    }

    stop ();
  #ifdef LIBMARY_MT_SAFE
    multi_thread->join ();
  #endif

    return Result::Success;
}

void
ServerApp::stop ()
{
    should_stop.set (1);
    if (!poll_group->trigger ())
        logE_ (_func, "poll_group.trigger() failed: ", exc->toString());

  #ifdef LIBMARY_MT_SAFE
    mutex.lock ();

    // FIXME If stop() is called before all thread funcs have added corresponding
    //       entries to 'thread_data_list', then we'll loose a thread and then
    //       fail to join() it afterwards, causing hangup.
    {
        ThreadDataList::iter iter (thread_data_list);
        while (!thread_data_list.iter_done (iter)) {
            ThreadData * const thread_data = thread_data_list.iter_next (iter)->data;
            if (!thread_data->poll_group->trigger ())
                logE_ (_func, "poll_group.trigger() failed: ", exc->toString());
        }
    }

    mutex.unlock ();
  #endif
}

void
ServerApp::release ()
{
    dcs_queue->release ();
}

ServerApp::ServerApp (EmbedContainer * const embed_container,
                      Count            const num_threads)
    : ServerContext      (embed_container),
      event_informer     (this /* outer_object */, &mutex),

      timers             (this /* embed_container */),
      poll_group         (this /* embed_container */),
      deferred_processor (this /* embed_container */),
      dcs_queue          (this /* embed_container */),

      main_thread_ctx    (this /* embed_container */),

      init_tlocal (NULL)

    #ifdef LIBMARY_MT_SAFE
      , num_threads     (1)
      , thread_selector (NULL)
    #endif
{
  #ifdef LIBMARY_MT_SAFE
    multi_thread = grab (new (std::nothrow) MultiThread (
            NULL /* embed_container */,
            num_threads,
            CbDesc<Thread::ThreadFunc> (threadFunc, this, this)));
  #else
    (void) num_threads;
  #endif
}

ServerApp::~ServerApp ()
{
    logD_ (_func_);

    release ();
}

}

