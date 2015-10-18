/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/moment_server.h>

#include <moment/channel.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_ctl ("moment.channel", LogLevel::I);

Playback::Frontend Channel::playback_frontend = {
    startPlaybackItem
};

void
Channel::startPlaybackItem (Playlist::Item          * const playlist_item,
                            Time                      const seek,
                            Playback::AdvanceTicket * const advance_ticket,
                            void                    * const _self)
{
    Channel * const self = static_cast <Channel*> (_self);

    if (logLevelOn (ctl, LogLevel::Debug)) {
        if (playlist_item) {
            logD_ (_self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\"");
            playlist_item->playback_item->dump (LogLevel::Debug, _self_func, "playback_item: ");
        } else {
            logD_ (_self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\": gap");
        }
    }

    self->mutex.lock ();

    Ref<VideoStream> old_stream;
    self->closeStream (&old_stream);

    self->cur_item = (playlist_item ? playlist_item->playback_item : NULL);
    self->advance_ticket = advance_ticket;

    self->createStream (seek);

    Ref<VideoStream> const new_stream = self->stream;
    self->mutex.unlock ();

    self->fireStartItem (new_stream, (old_stream ? old_stream : new_stream));

    if (old_stream)
        old_stream->close ();
}

VideoStream::StateEvents const Channel::stream_state_events = {
    NULL /* closed */,
    numWatchersChanged,
    NULL /* publishingStatusChanged */,
    NULL /* media_desc */
};

void
Channel::numWatchersChanged (Count   const num_watchers,
                             void  * const _stream_data)
{
    logD_ (_func, num_watchers);

    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data) {
        self->mutex.unlock ();
        return;
    }

    stream_data->num_watchers = num_watchers;

    if (num_watchers == 0) {
        if (!self->connect_on_demand_timer) {
            logD_ (_self_func, "starting connect-on-dmenad timer, timeout: ", self->channel_opts->connect_on_demand_timeout);
            self->connect_on_demand_timer = self->timers->addTimer (
                    CbDesc<Timers::TimerCallback> (connectOnDemandTimerTick,
                                                   stream_data /* cb_data */,
                                                   self        /* guard_obj */,
                                                   stream_data /* ref_data */),
                    self->channel_opts->connect_on_demand_timeout,
                    false /* periodical */);
        }
    } else {
        if (self->connect_on_demand_timer) {
            self->timers->deleteTimer (self->connect_on_demand_timer);
            self->connect_on_demand_timer = NULL;
        }

        if (!self->stream_source && !self->source_gap) {
            logD_ (_func, "connecting on demand");
            mt_unlocks (mutex) self->doRestartStream ();
            return;
        }
    }

    self->mutex.unlock ();
}

void
Channel::connectOnDemandTimerTick (void * const _stream_data)
{
    StreamData * const stream_data = static_cast <StreamData*> (_stream_data);
    Channel * const self = stream_data->channel;

    logD_ (_self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\"");

    self->mutex.lock ();
    if (stream_data != self->cur_stream_data) {
        self->mutex.unlock ();
        return;
    }

    Ref<VideoStream> old_stream;
    if (stream_data->num_watchers == 0) {
        logD_ (_self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\": ",
               "disconnecting on timeout");
        self->closeStream (&old_stream);
    }
    self->mutex.unlock ();

    if (old_stream)
        old_stream->close ();
}

mt_mutex (mutex) void
Channel::beginConnectOnDemand ()
{
    assert (stream);

    if (!channel_opts->connect_on_demand || source_gap)
        return;

    stream->lock ();

    cur_stream_data->num_watchers = stream->getNumWatchers_locked();

    if (cur_stream_data->num_watchers == 0) {
        logD_ (_func, "starting timer, timeout: ", channel_opts->connect_on_demand_timeout);
        connect_on_demand_timer = timers->addTimer (
                CbDesc<Timers::TimerCallback> (connectOnDemandTimerTick,
                                               cur_stream_data /* cb_data */,
                                               this            /* guard_obj */,
                                               cur_stream_data /* ref_data */),
                channel_opts->connect_on_demand_timeout,
                false /* periodical */);
    }

    stream_state_events_sbn = stream->getStateEventInformer()->subscribe_locked (
            CbDesc<VideoStream::StateEvents> (
                    &stream_state_events,
                    cur_stream_data /* cb_data */,
                    this            /* guard_obj */,
                    cur_stream_data /* ref_data */));

    stream->unlock ();
}

void
Channel::setStreamParameters (VideoStream * const mt_nonnull stream)
{
    Ref<StreamParameters> const stream_params = grabNew <StreamParameters> ();
    if (cur_item){
        if (cur_item->no_audio)
            stream_params->setParam ("no_audio", "true");
        if (cur_item->no_video)
            stream_params->setParam ("no_video", "true");

        logD_ (_this_func, "stream 0x", fmt_hex, (UintPtr) stream, ": no_audio: ", cur_item->no_audio, ", no_video: ", cur_item->no_video);
    }

    stream->setStreamParameters (stream_params);
    stream->setDomainOptions (channel_opts->domain_opts);
}

mt_mutex (mutex) void
Channel::createStream (Time const initial_seek)
{
    logD (ctl, _this_func_);

    assert (!stream_source);
    assert (!got_video);

    Ref<VideoStream> bind_stream = stream;
    if (channel_opts->continuous_playback) {
        bind_stream = grabNewObject <VideoStream> (deferred_processor);
        stream->bindToStream (bind_stream, bind_stream, true, true);
    }

    if (stream_start_time == 0)
        stream_start_time = getTime();

    if (!cur_item) {
        source_gap = true;
        return;
    }

    source_gap = false;
    beginConnectOnDemand ();

    Ref<StreamGroup> const stream_group = grabNewObject <StreamGroup> ();
    stream_group->addStream (bind_stream);
    stream_group->start ();

    if (!channel_opts->connect_on_demand
        || cur_stream_data->num_watchers > 0)
    {
        if (Ref<SourceManager> const source_manager = weak_source_manager.getRef()) {
            cur_source_data = grabNew <SourceData> (this);
            stream_source = source_manager->createStreamSource (
                                    CbDesc<StreamSource::Frontend> (
                                            &stream_source_frontend,
                                            cur_source_data /* cb_data */,
                                            this            /* guard_obj */,
                                            cur_source_data /* ref_data */),
                                    timers,
                                    deferred_processor,
                                    page_pool,
                                    stream_group,
                                    initial_seek,
                                    channel_opts,
                                    cur_item);
            if (!stream_source) {
              /* TODO Handle !stream_source case */
            }
        } else {
            logD_ (_this_func, "source_manager gone");
        }
    } else {
        logD_ (_this_func, "connect-on-demand: no watchers, deferring");
    }
}

mt_mutex (mutex) void
Channel::closeStream (Ref<VideoStream> * const mt_nonnull ret_old_stream)
{
    logD (ctl, _this_func, "channel \"", String::mem (channel_opts->channel_name), "\"");

    stopConnectOnDemand ();
    releaseSource ();
    replaceStream (ret_old_stream);
}

mt_mutex (mtuex) void
Channel::stopConnectOnDemand ()
{
    if (connect_on_demand_timer) {
        timers->deleteTimer (connect_on_demand_timer);
        connect_on_demand_timer = NULL;
    }
}

mt_mutex (mutex) void
Channel::releaseSource ()
{
    got_video = false;

    if (stream_source) {
	{
	    StreamSource::TrafficStats traffic_stats;
	    stream_source->getTrafficStats (&traffic_stats);

	    rx_bytes_accum += traffic_stats.rx_bytes;
	    rx_audio_bytes_accum += traffic_stats.rx_audio_bytes;
	    rx_video_bytes_accum += traffic_stats.rx_video_bytes;
	}

	stream_source->releasePipeline ();
	stream_source = NULL;
    }
}

mt_mutex (mutex) void
Channel::releaseStream (Ref<Stream> * const mt_nonnull ret_old_stream)
{
    *ret_old_stream = NULL;

    if (stream) {
        *ret_old_stream = stream;

        if (stream_state_events_sbn) {
            stream->getStateEventInformer()->unsubscribe (stream_state_events_sbn);
            stream_state_events_sbn = NULL;
        }

      // Note that moment->removeVideoStream() is not called here.

        stream = NULL;
    }

    cur_stream_data = NULL;
}

mt_mutex (mutex) void
Channel::replaceStream (Ref<Stream> * const mt_nonnull ret_old_stream)
{
    *ret_old_stream = NULL;

    if (!stream
        || (   !channel_opts->keep_video_stream
            && !channel_opts->continuous_playback))
    {
        releaseStream (ret_old_stream);

        cur_stream_data = grabNew <StreamData> (this);
        logD_ (_this_func, "channel \"", String::mem (channel_opts->channel_name), "\" ",
               "new stream_data 0x", fmt_hex, (UintPtr) cur_stream_data.ptr());

        stream = grabNewObject <Stream> (deferred_processor);
        setStreamParameters (stream);

        if (Ref<StreamManager> const stream_manager = weak_stream_manager.getRef()) {
            stream_manager->lock ();
            if (stream_key) {
                stream_manager->removeStream_locked (stream_key);
            }
            stream_key = stream_manager->addStream_locked (stream, channel_opts->channel_name->mem());
            stream_manager->unlock ();
        } else {
            logD_ (_this_func, "stream_manager gone");
        }
    }
}

mt_unlocks (mutex) void
Channel::doRestartStream ()
{
    logD (ctl, _this_func_);

    Ref<VideoStream> old_stream;
    closeStream (&old_stream);

    // TODO Set correct initial seek.
    createStream (0 /* initial_seek */);

    Ref<VideoStream> const new_stream = stream;
    mutex.unlock ();

    if (old_stream) {
        // TODO this is racy: fireNewVideoStream() invocations should be serialized
        fireNewVideoStream (new_stream, old_stream);
        old_stream->close ();
    }
}

StreamSource::Frontend Channel::stream_source_frontend = {
    sourceError,
    sourceEos,
    noVideo,
    gotVideo
};

void
Channel::doSourceEos (SourceData * const mt_nonnull source_data)
{
    mutex.lock ();
    source_data->source_closed = true;
    if (source_data != cur_source_data) {
	mutex.unlock ();
	return;
    }

    Ref<Playback::AdvanceTicket> const tmp_advance_ticket = advance_ticket;
    mutex.unlock ();

    playback->advance (tmp_advance_ticket);
}

void
Channel::sourceError (void * const _source_data)
{
    SourceData * const source_data = static_cast <SourceData*> (_source_data);
    Channel * const self = source_data->channel;

    logD (ctl, _self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\" "
          "source_data 0x", fmt_hex, (UintPtr) source_data);

    self->doSourceEos (source_data);
}

void
Channel::sourceEos (void * const _source_data)
{
    SourceData * const source_data = static_cast <SourceData*> (_source_data);
    Channel * const self = source_data->channel;

    logD (ctl, _self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\" "
          "source_data 0x", fmt_hex, (UintPtr) source_data);

    self->doSourceEos (source_data);
}

void
Channel::noVideo (void * const _source_data)
{
    SourceData * const source_data = static_cast <SourceData*> (_source_data);
    Channel * const self = source_data->channel;

    self->mutex.lock ();
    if (source_data != self->cur_source_data) {
        self->mutex.unlock ();
        return;
    }

    if (source_data->source_closed) {
        self->mutex.unlock ();
        logD_ (_self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\" "
               "source_data 0x", fmt_hex, (UintPtr) source_data,
               ": source_closed");
        return;
    }

    logW_ (_self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\" "
           "(\"", self->channel_opts->channel_title, "\"): "
           "no video, restarting stream");
    mt_unlocks (mutex) self->doRestartStream ();
}

void
Channel::gotVideo (void * const _source_data)
{
    SourceData * const source_data = static_cast <SourceData*> (_source_data);
    Channel * const self = source_data->channel;

    logD (ctl, _self_func, "channel \"", String::mem (self->channel_opts->channel_name), "\"");

    self->mutex.lock ();
    if (source_data != self->cur_source_data
        || source_data->source_closed)
    {
        self->mutex.unlock ();
        return;
    }

    self->got_video = true;
    self->mutex.unlock ();
}

void
Channel::restartStream ()
{
    logD (ctl, _this_func, "channel \"", String::mem (channel_opts->channel_name), "\"");

    mutex.lock ();
    mt_unlocks (mutex) doRestartStream ();
}

bool
Channel::isSourceOnline ()
{
    mutex.lock ();
    bool const res = got_video;
    mutex.unlock ();
    return res;
}

void
Channel::getTrafficStats (TrafficStats * const ret_traffic_stats)
{
    mutex.lock ();

    StreamSource::TrafficStats stream_tstat;
    if (stream_source)
	stream_source->getTrafficStats (&stream_tstat);
    else
	stream_tstat.reset ();

    ret_traffic_stats->rx_bytes = rx_bytes_accum + stream_tstat.rx_bytes;
    ret_traffic_stats->rx_audio_bytes = rx_audio_bytes_accum + stream_tstat.rx_audio_bytes;
    ret_traffic_stats->rx_video_bytes = rx_video_bytes_accum + stream_tstat.rx_video_bytes;
    {
	Time const cur_time = getTime();
	if (cur_time > stream_start_time)
	    ret_traffic_stats->time_elapsed = cur_time - stream_start_time;
	else
	    ret_traffic_stats->time_elapsed = 0;
    }

    mutex.unlock ();
}

void
Channel::resetTrafficStats ()
{
    mutex.lock ();

    if (stream_source)
	stream_source->resetTrafficStats ();

    rx_bytes_accum = 0;
    rx_audio_bytes_accum = 0;
    rx_video_bytes_accum = 0;

    stream_start_time = getTime();

    mutex.unlock ();
}

mt_const void
Channel::init (MomentServer   * const mt_nonnull moment,
               ChannelOptions * const mt_nonnull channel_opts)
{
    this->channel_opts = channel_opts;

    this->weak_source_manager = moment->getSourceManager();
    this->weak_stream_manager = moment->getStreamManager();

    this->timers             = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();
    this->deferred_processor = moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor();
    this->page_pool = moment->getPagePool();

    playback->init (CbDesc<Playback::Frontend> (&playback_frontend, this, this),
                    moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                    channel_opts->min_playlist_duration_sec,
                    channel_opts->pause_time_sec);
}

Channel::Channel (EmbedContainer * const embed_container)
    : Object (embed_container),

      event_informer (this /* outer_object */, &mutex),
      playback       (this /* embed_container */),
        
      source_gap (true),

      destroyed (false),

      got_video (false),

      stream_start_time (0),

      connect_on_demand_timer (NULL),

      rx_bytes_accum (0),
      rx_audio_bytes_accum (0),
      rx_video_bytes_accum (0)
{
    logD (ctl, _this_func_);
}

void
Channel::doDestroy (bool const from_dtor)
{
    mutex.lock ();
    destroyed = true;

    mt_unlocks_locks (mutex) fireDestroyed_locked ();

    // If we're in a dtor, then timers and subscription keys are invalid
    // at this point, since deletion callbacks have already been called.
    if (from_dtor) {
        connect_on_demand_timer = NULL;
        stream_state_events_sbn = NULL;
    }

    Ref<VideoStream> old_stream = stream;
    stopConnectOnDemand ();
    releaseSource ();
    releaseStream (&old_stream);

    if (stream_key) {
        if (Ref<StreamManager> const stream_manager = weak_stream_manager.getRef()) {
            stream_manager->removeStream (stream_key);
        } else {
            logD_ (_this_func, "stream_manager gone");
        }

        stream_key = StreamManager::StreamKey();
    }

    mutex.unlock ();

    if (old_stream)
        old_stream->close ();
}

void
Channel::destroy ()
{
    doDestroy (false /* from_dtor */);
}

Channel::~Channel ()
{
    logD (ctl, _this_func_);

    doDestroy (true /* from_dtor */);
}

}

