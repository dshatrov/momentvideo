/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/fetch_agent.h>


namespace Moment {

void
FetchAgent::startNewSession ()
{
    logD_ (_this_func_);

    Ref<Session> const session = grabNewObject <Session> ();
    session->valid = true;
    session->weak_fetch_agent = this;
    session->connect_begin_time = getTimeMilliseconds();

    mutex.lock ();
    if (bound_stream) {
        fetch_stream = grabNewObject <MediaStream> (deferred_processor);
        bound_stream->bindToStream (fetch_stream, fetch_stream, true, true);
    } else {
        fetch_stream = NULL;
    }

    session->fetch_conn =
            fetch_proto->newConnection (
                    channel_opts,
                    playback_item,
                    fetch_stream,
                    uri->mem(),
                    CbDesc<FetchConnection::FetchConnectionFrontend> (&fetch_conn_frontend, session, session));

    if (reconnect_timer) {
        timers->deleteTimer (reconnect_timer);
        reconnect_timer = NULL;
    }

    cur_session = session;
    mutex.unlock ();

    if (!session->fetch_conn->start ())
        deferred_reg.scheduleTask (&disconnected_task, false /* permanent */);
}

void
FetchAgent::reconnectTimerTick (void * const _self)
{
    FetchAgent * const self = static_cast <FetchAgent*> (_self);

    logD_ (_self_func_);

    self->mutex.lock ();

    assert (self->reconnect_timer);
    self->timers->deleteTimer (self->reconnect_timer);
    self->reconnect_timer = NULL;

    self->mutex.unlock ();

    self->startNewSession ();
}

mt_unlocks (mutex) void
FetchAgent::doDisconnected (Session * const mt_nonnull session,
                            bool      const report_events)
{
    logD_ (_this_func_);

    if (!session || session != cur_session || !session->valid) {
        mutex.unlock ();
        return;
    }
    session->valid = false;

    cur_session = NULL;
    mutex.unlock ();

    if (!do_reconnects) {
        if (report_events) {
            if (frontend)
                frontend.call (frontend->disconnected);

            if (stream_source_frontend)
                stream_source_frontend.call (stream_source_frontend->error);
        }

        return;
    }

    Time time_delta = 0;
    if (reconnect_interval_millisec) {
        Time const cur_time_millisec = getTimeMilliseconds();
        if (cur_time_millisec > session->connect_begin_time) {
            Time const elapsed = cur_time_millisec - session->connect_begin_time;
            if (elapsed < reconnect_interval_millisec)
                time_delta = reconnect_interval_millisec - elapsed;
        }
    }

    if (time_delta == 0) {
        startNewSession ();
    } else {
        mutex.lock ();
        reconnect_timer =
                timers->addTimer_microseconds (
                        CbDesc<Timers::TimerCallback> (reconnectTimerTick, this, this),
                        time_delta * 1000,
                        false /* periodical */,
                        true  /* auto_delete */,
                        false /* delete_after_tick */);
        mutex.unlock ();
    }
}

bool
FetchAgent::disconnectedTask (void * const _self)
{
    FetchAgent * const self = static_cast <FetchAgent*> (_self);

    logD_ (_self_func_);

    self->mutex.lock ();
    if (!self->cur_session) {
        self->mutex.unlock ();
        return false /* do not reschedule */;
    }

    mt_unlocks (mutex) self->doDisconnected (self->cur_session, true /* report_events */);

    return false /* do not reschedule */;
}

FetchConnection::FetchConnectionFrontend const FetchAgent::fetch_conn_frontend (
    fetchConnDisconnected,
    fetchConnGotVideo
);

void
FetchAgent::fetchConnDisconnected (DisconnectReason   const /* disconnect_reason */,
                                   void             * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<FetchAgent> const self = session->weak_fetch_agent.getRef();
    if (!self)
        return;

    logD_ (_self_func_);

    self->mutex.lock ();
    mt_unlocks (mutex) self->doDisconnected (session, true /* report_events */);
}

void
FetchAgent::fetchConnGotVideo (bool   const got_video,
                               void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<FetchAgent> const self = session->weak_fetch_agent.getRef();
    if (!self)
        return;

    self->mutex.lock ();
    if (session != self->cur_session || !session->valid)
    {
        self->mutex.unlock ();
        return;
    }

    self->mutex.unlock ();

    if (self->stream_source_frontend) {
        if (got_video)
            self->stream_source_frontend.call (self->stream_source_frontend->gotVideo);
        else
            self->stream_source_frontend.call (self->stream_source_frontend->noVideo);
    }
}

void
FetchAgent::createPipeline ()
{
    startNewSession ();
}

void
FetchAgent::releasePipeline ()
{
    mutex.lock ();
    mt_unlocks (mutex) doDisconnected (cur_session, false /* report_events */);
}

void
FetchAgent::getTrafficStats (StreamSource::TrafficStats * const mt_nonnull /* ret_traffic_stats */)
{
  // TODO
}

void
FetchAgent::resetTrafficStats ()
{
  // TODO
}

void
FetchAgent::init (ServerContext  * const mt_nonnull server_ctx,
                  ChannelOptions * const mt_nonnull channel_opts,
                  PlaybackItem   * const mt_nonnull playback_item,
                  FetchProtocol  * const mt_nonnull fetch_proto,
                  VideoStream    * const stream,
                  ConstMemory      const uri,
                  bool             const do_reconnects,
                  bool             const autostart,
                  Time             const reconnect_interval_millisec,
                  CbDesc<Frontend> const &frontend,
                  CbDesc<StreamSource::Frontend> const &stream_source_frontend)
{
    logD_ (_this_func_);

    this->timers        = server_ctx->getMainThreadContext()->getTimers();
    this->deferred_processor = server_ctx->getMainThreadContext()->getDeferredProcessor();
    this->channel_opts  = channel_opts;
    this->playback_item = playback_item;
    this->fetch_proto   = fetch_proto;
    this->uri           = newString (uri);
    this->do_reconnects = do_reconnects;
    this->reconnect_interval_millisec = reconnect_interval_millisec;
    this->frontend      = frontend;
    this->stream_source_frontend = stream_source_frontend;

    deferred_reg.setDeferredProcessor (server_ctx->getMainThreadContext()->getDeferredProcessor());

    bound_stream = stream;
    if (bound_stream) {
        fetch_stream = grabNewObject <MediaStream> (deferred_processor);
        bound_stream->bindToStream (fetch_stream, fetch_stream, true, true);
    }

    if (autostart)
        startNewSession ();
}

FetchAgent::FetchAgent (EmbedContainer * const embed_container)
    : StreamSource (embed_container),
      reconnect_interval_millisec (0)
{
    disconnected_task.cb = CbDesc<DeferredProcessor::TaskCallback> (disconnectedTask, this, this);
}

}

