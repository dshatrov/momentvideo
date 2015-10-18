/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/recorder.h>


using namespace M;

namespace Moment {

Playback::Frontend Recorder::playback_frontend = {
    startPlaybackItem
};

void
Recorder::startPlaybackItem (Playlist::Item           * const playlist_item,
                             Time                       const /* seek */,
                             Playback::AdvanceTicket  * const /* advance_ticket */,
                             void                     * const _self)
{
    Recorder * const self = static_cast <Recorder*> (_self);

    Ref<Channel> channel;
    ConstMemory channel_name;
    if (playlist_item && playlist_item->id) {
        Ref<ChannelManager> const channel_manager = self->weak_channel_manager.getRef ();
        if (channel_manager) {
            channel_manager->channelManagerLock ();
            {
                ChannelManager::ChannelInfo channel_info;
                channel_name = String::mem (playlist_item->id);
                if (channel_manager->getChannelInfo_locked (channel_name, &channel_info))
                    channel = channel_info.channel;
            }
            channel_manager->channelManagerUnlock ();
        } else {
            logD_ (_self_func, "channel_manager gone");
        }
    }

    self->mutex.lock ();

    self->cur_recording_ticket = NULL;
    self->cur_channel_name = NULL;
    self->weak_cur_channel = NULL;
    self->weak_cur_video_stream = NULL;

    {
        Ref<Channel> const channel = self->weak_cur_channel.getRef ();
        if (channel && self->channel_sbn) {
            channel->getEventInformer()->unsubscribe (self->channel_sbn);
        }
        self->channel_sbn = GenericInformer::SubscriptionKey ();
    }

    self->doStopItem ();
    self->recording_now = false;

    if (!playlist_item) {
        self->mutex.unlock ();
        logD_ (_self_func, "gap");
        return;
    }

    if (!playlist_item->id) {
        self->mutex.unlock ();
        logW_ (_self_func, "channel name is not specified for a recorder item");
        return;
    }

    if (!channel) {
        self->mutex.unlock ();
        logD_ (_func, "channel \"", channel_name, "\" not found");
        return;
    }

    self->recording_now = true;
    self->cur_channel_name = st_grab (new (std::nothrow) String (channel_name));
    self->weak_cur_channel = channel;

    self->cur_recording_ticket = grab (new (std::nothrow) RecordingTicket);
    self->cur_recording_ticket->recorder = self;

    self->channel_sbn = channel->getEventInformer()->subscribe (CbDesc<Channel::ChannelEvents> (
            &channel_events,
            self->cur_recording_ticket /* cb_data */,
            self /* guard_obj */,
            self->cur_recording_ticket /* ref_data */));

    self->doStartItem ();

    self->mutex.unlock ();
}

Channel::ChannelEvents Recorder::channel_events = {
    startChannelItem,
    stopChannelItem,
    newVideoStream,
    NULL /* destroyed */
};

void
Recorder::startChannelItem (MediaStream * const /* new_stream */,
                            MediaStream * const /* old_stream */,
                            void        * const _recording_ticket)
{
    RecordingTicket * const recording_ticket =
            static_cast <RecordingTicket*> (_recording_ticket);
    Recorder * const self = recording_ticket->recorder;

    logD_ (_func_);

    self->mutex.lock ();
    if (self->cur_recording_ticket != recording_ticket) {
        self->mutex.unlock ();
        logD_ (_func, "recording ticket mismatch");
        return;
    }

    self->doStopItem ();

    Ref<Channel> const channel = self->weak_cur_channel.getRef();
    if (!channel) {
        self->mutex.unlock ();
        logD_ (_func, "channel gone");
        return;
    }

    self->doStartItem ();

    self->mutex.unlock ();
}

void
Recorder::stopChannelItem (MediaStream * const /* new_stream */,
                           MediaStream * const /* old_stream */,
                           void        * const _recording_ticket)
{
    RecordingTicket * const recording_ticket =
            static_cast <RecordingTicket*> (_recording_ticket);
    Recorder * const self = recording_ticket->recorder;

    self->mutex.lock ();
    if (self->cur_recording_ticket != recording_ticket) {
        self->mutex.unlock ();
        return;
    }

    self->doStopItem ();

    self->mutex.unlock ();
}

void
Recorder::newVideoStream (MediaStream * const /* new_stream */,
                          MediaStream * const /* old_stream */,
                          void        * const _recording_ticket)
{
    RecordingTicket * const recording_ticket =
            static_cast <RecordingTicket*> (_recording_ticket);
    Recorder * const self = recording_ticket->recorder;

    self->mutex.lock ();
    if (self->cur_recording_ticket != recording_ticket) {
        self->mutex.unlock ();
        return;
    }

    // Verifying that stream has actually changed.
    if (self->cur_channel_name) {
        if (Ref<StreamManager> const stream_manager = self->weak_stream_manager.getRef ()) {
            Ref<VideoStream> const video_stream = stream_manager->getStream (self->cur_channel_name->mem());
            if (video_stream) {
                Ref<VideoStream> const cur_video_stream = self->weak_cur_video_stream.getRef ();
                if (cur_video_stream && cur_video_stream == video_stream.ptr()) {
                  // Media stream has not changed.
                    self->mutex.unlock ();
                    return;
                }
            }
        } else {
            logD_ (_self_func, "stream_manager gone");
            return;
        }
    }

    self->doStopItem ();

    Ref<Channel> const channel = self->weak_cur_channel.getRef();
    if (!channel) {
        self->mutex.unlock ();
        return;
    }

    self->doStartItem ();

    self->mutex.unlock ();
}

mt_mutex (mutex) void
Recorder::doStartItem ()
{
    logD_ (_func_);

    if (!recording_now) {
        logD_ (_func, "!recording_now");
        return;
    }

    assert (cur_channel_name);

    Ref<VideoStream> video_stream;
    if (Ref<StreamManager> const stream_manager = weak_stream_manager.getRef ()) {
        video_stream = stream_manager->getStream (cur_channel_name->mem());
    } else {
        logD_ (_this_func, "stream_manager gone");
        return;
    }

    if (!video_stream) {
        logW_ (_func, "stream \"", cur_channel_name->mem(), "\" not found");
        return;
    }

    weak_cur_video_stream = video_stream;
    recorder->setVideoStream (video_stream);

    {
        Format fmt;
        fmt.num_base = 10;
        fmt.min_digits = 2;

        LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
        StRef<String> const filename = makeString (
                fmt,
                filename_prefix->mem(),
                filename_prefix->mem().len() > 0 ? "_" : "",
                tlocal->localtime.tm_year + 1900, "-",
                tlocal->localtime.tm_mon + 1, "-",
                tlocal->localtime.tm_mday, "_",
                tlocal->localtime.tm_hour, "-",
                tlocal->localtime.tm_min, "-",
                tlocal->localtime.tm_sec,
                ".flv");

        logD_ (_func, "calling recorder.start(), filename: ", filename->mem());
        recorder->start (filename->mem());
    }
}

mt_mutex (mutex) void
Recorder::doStopItem ()
{
    logD_ (_func_);

    if (!recording_now)
        return;

    recorder->stop ();
}

mt_const void
Recorder::init (MomentServer * const moment,
                PagePool     * const page_pool,
                ConstMemory    const filename_prefix,
                Uint64         const min_playlist_duration_sec,
                Uint64         const pause_time_sec)
{
    this->weak_moment          = moment;
    this->weak_stream_manager  = moment->getStreamManager();
    this->weak_channel_manager = moment->getChannelManager();

    this->filename_prefix = st_grab (new (std::nothrow) String (filename_prefix));

    playback->init (CbDesc<Playback::Frontend> (&playback_frontend, this, this),
                    moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                    min_playlist_duration_sec,
                    pause_time_sec);

    flv_muxer.setPagePool (page_pool);

    {
        ServerThreadContext *thread_ctx =
                moment->getRecorderThreadPool()->grabThreadContext (filename_prefix);
        if (thread_ctx) {
            recorder_thread_ctx = thread_ctx;
        } else {
            logE_ (_func, "couldn't get recorder thread context: ", exc->toString());
            thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
        }

        // TODO recorder frontend + error reporting
        recorder->init (CbDesc<AvRecorder::Frontend> (),
                        thread_ctx,
                        moment->getStorage(),
                        &flv_muxer,
                        (Uint64) -1 /* recording_limit */);
    }
}

Recorder::Recorder (EmbedContainer * const embed_container)
    : Object              (embed_container),
      recorder_thread_ctx (NULL),
      playback            (this /* embed_container */),
      recorder            (this /* embed_container */),
      recording_now       (false)
{
}

Recorder::~Recorder ()
{
    mutex.lock ();
#if 0
// Wrong: channel_sbn has been invalidated by deletion callback.
    {
        Ref<Channel> const channel = weak_cur_channel.getRef ();
        if (channel && channel_sbn) {
            channel->getEventInformer()->unsubscribe (channel_sbn);
        }
        channel_sbn = GenericInformer::SubscriptionKey ();
    }
#endif
    doStopItem ();
    mutex.unlock ();

    if (recorder_thread_ctx) {
        if (Ref<MomentServer> const moment = weak_moment.getRef ()) {
            moment->getRecorderThreadPool()->releaseThreadContext (recorder_thread_ctx);
        }
        recorder_thread_ctx = NULL;
    }
}

}

