/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/inc.h>

#include <moment/stream.h>


using namespace M;

static LogGroup libMary_logGroup_stream ("media_stream", LogLevel::I);

namespace Moment {

MOMENT__VIDEO_STREAM

namespace {
struct InformMediaDesc_Data
{
    MediaDesc_Audio * const audio_desc;
    bool              const set_audio;
    MediaDesc_Video * const video_desc;
    bool              const set_video;

    InformMediaDesc_Data (MediaDesc_Audio * const audio_desc,
                          bool              const set_audio,
                          MediaDesc_Video * const video_desc,
                          bool              const set_video)
        : audio_desc (audio_desc),
          set_audio  (set_audio),
          video_desc (video_desc),
          set_video  (set_video)
    {}
};
}

void
VideoStream::informMediaDesc (Events * const events,
                              void   * const cb_data,
                              void   * const _inform_data)
{
    if (events->mediaDesc) {
        InformMediaDesc_Data * const inform_data =
                static_cast <InformMediaDesc_Data*> (_inform_data);
        logS (stream, _func_);
        events->mediaDesc (inform_data->audio_desc,
                           inform_data->set_audio,
                           inform_data->video_desc,
                           inform_data->set_video,
                           cb_data);
    }
}

void
VideoStream::stateInformMediaDesc (StateEvents * const state_events,
                                   void        * const cb_data,
                                   void        * const _inform_data)
{
    if (state_events->mediaDesc) {
        InformMediaDesc_Data * const inform_data =
                static_cast <InformMediaDesc_Data*> (_inform_data);
        logS (stream, _func_);
        state_events->mediaDesc (inform_data->audio_desc,
                                 inform_data->set_audio,
                                 inform_data->video_desc,
                                 inform_data->set_video,
                                 cb_data);
    }
}

namespace {
struct InformAudioMessage_Data
{
    AudioMessage *msg;

    InformAudioMessage_Data (AudioMessage * const msg)
        : msg (msg)
    {}
};
}

void
VideoStream::informAudioMessage (Events * const events,
                                 void   * const cb_data,
                                 void   * const _inform_data)
{
    if (events->audioMessage) {
        InformAudioMessage_Data * const inform_data =
                static_cast <InformAudioMessage_Data*> (_inform_data);
        logS (stream, _func, "events 0x", fmt_hex, (UintPtr) events, fmt_def, ", "
              "pts ", inform_data->msg->pts_nanosec, ", "
              "dts ", inform_data->msg->dts_nanosec, ", "
              "len ", inform_data->msg->msg_len);
        events->audioMessage (inform_data->msg, cb_data);
    }
}

namespace {
struct InformVideoMessage_Data
{
    VideoMessage *msg;

    InformVideoMessage_Data (VideoMessage * const msg)
        : msg (msg)
    {}
};
}

void
VideoStream::informVideoMessage (Events * const events,
                                 void   * const cb_data,
                                 void   * const _inform_data)
{
    if (events->videoMessage) {
        InformVideoMessage_Data * const inform_data =
                static_cast <InformVideoMessage_Data*> (_inform_data);
        logS (stream, _func, "events 0x", fmt_hex, (UintPtr) events, fmt_def, ", "
              "pts ", inform_data->msg->pts_nanosec, ", "
              "dts ", inform_data->msg->dts_nanosec, ", "
              "len ", inform_data->msg->msg_len);
        events->videoMessage (inform_data->msg, cb_data);
    }
}

namespace {
struct InformRtmpDataMessage_Data
{
    MediaMessage *msg;

    InformRtmpDataMessage_Data (MediaMessage * const msg)
        : msg (msg)
    {}
};
}

void
VideoStream::informRtmpDataMessage (Events * const events,
                                    void   * const cb_data,
                                    void   * const _inform_data)
{
    if (events->rtmpDataMessage) {
        InformRtmpDataMessage_Data * const inform_data =
                static_cast <InformRtmpDataMessage_Data*> (_inform_data);
        logS (stream, _func, "events 0x", fmt_hex, (UintPtr) events, fmt_def, ", "
              "pts ", inform_data->msg->pts_nanosec, ", "
              "dts ", inform_data->msg->dts_nanosec, ", "
              "len ", inform_data->msg->msg_len);
        events->rtmpDataMessage (inform_data->msg, cb_data);
    }
}

namespace {
struct InformRtmpCommandMessage_Data
{
    RtmpConnection *conn;
    MediaMessage   *msg;
    ConstMemory     method_name;
    AmfDecoder     *amf_decoder;

    InformRtmpCommandMessage_Data (RtmpConnection * const conn,
                                   MediaMessage   * const msg,
                                   ConstMemory      const method_name,
                                   AmfDecoder     * const amf_decoder)
        : conn (conn),
          msg (msg),
          method_name (method_name),
          amf_decoder (amf_decoder)
    {}
};
}

void
VideoStream::informRtmpCommandMessage (Events * const events,
                                       void   * const cb_data,
                                       void   * const _inform_data)
{
    // TODO Save/restore amf_decoder state between callback invocations.
    //      Viable option - abstract away the parsing process.
    if (events->rtmpCommandMessage) {
        InformRtmpCommandMessage_Data * const inform_data =
                static_cast <InformRtmpCommandMessage_Data*> (_inform_data);
        events->rtmpCommandMessage (inform_data->conn,
                                    inform_data->msg,
                                    inform_data->method_name,
                                    inform_data->amf_decoder,
                                    cb_data);
    }
}

void
VideoStream::informClosed (Events * const events,
                           void   * const cb_data,
                           void   * const /* inform_data */)
{
    if (events->closed)
        events->closed (cb_data);
}

void
VideoStream::stateInformClosed (StateEvents * const state_events,
                                void        * const cb_data,
                                void        * const /* inform_data */)
{
    if (state_events->closed)
        state_events->closed (cb_data);
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::reportPendingEvents ()
{
    while (!pending_event_list.isEmpty()) {
        PendingEvent * const pending_event = pending_event_list.getFirst();
        pending_event_list.remove (pending_event);

        switch (pending_event->getType()) {
            case PendingEvent::t_MediaDesc: {
                PendingMediaDesc * const desc_frame = static_cast <PendingMediaDesc*> (pending_event);

                frame_saver.processMediaDesc (
                        desc_frame->audio_desc, desc_frame->set_audio, desc_frame->video_desc, desc_frame->set_video);

                InformMediaDesc_Data inform_data (
                        desc_frame->audio_desc, desc_frame->set_audio, desc_frame->video_desc, desc_frame->set_video);
                mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informMediaDesc, &inform_data);
                mt_async mt_unlocks_locks (mutex) state_informer.informAll_locked (stateInformMediaDesc, &inform_data);
            } break;
            case PendingEvent::t_Audio: {
                PendingAudioFrame * const audio_frame = static_cast <PendingAudioFrame*> (pending_event);

                frame_saver.processAudioFrame (&audio_frame->audio_msg);

                InformAudioMessage_Data inform_data (&audio_frame->audio_msg);
                mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informAudioMessage, &inform_data);
            } break;
            case PendingEvent::t_Video: {
                PendingVideoFrame * const video_frame = static_cast <PendingVideoFrame*> (pending_event);

                frame_saver.processVideoFrame (&video_frame->video_msg);

                InformVideoMessage_Data inform_data (&video_frame->video_msg);
                mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informVideoMessage, &inform_data);
            } break;
            case PendingEvent::t_Data: {
                PendingDataFrame * const data_frame = static_cast <PendingDataFrame*> (pending_event);

                InformRtmpDataMessage_Data inform_data (&data_frame->data_msg);
                mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informRtmpDataMessage, &inform_data);
            } break;
            case PendingEvent::t_Event_BeginPublishing: {
                firePublishingStatusChanged_locked (true);
            } break;
            case PendingEvent::t_Event_EndPublishing: {
                firePublishingStatusChanged_locked (false);
            } break;
        }

        delete pending_event;
    }

    pending_report_in_progress = false;
}

bool
VideoStream::reportPendingEventsTask (void * const _self)
{
    VideoStream * const self = static_cast <VideoStream*> (_self);

    self->mutex.lock ();
    if (self->pending_report_in_progress) {
        if (self->msg_inform_counter == 0) {
            ++self->msg_inform_counter;

            mt_async mt_unlocks_locks (mutex) self->reportPendingEvents ();

            --self->msg_inform_counter;
        }
    }
    self->mutex.unlock ();

    return false /* do not reschedule */;
}

mt_mutex (mutex) void
VideoStream::reportPendingEvents_deferred ()
{
    logD (stream, _this_func_);

    if (pending_report_in_progress) {
        logD (stream, _this_func, "pending_report_in_progress");
        return;
    }

  // Note that we should _always_ enter "pending report" mode when binding.
  // This ensures strict barrier between old streams' and new streams' events.

    pending_report_in_progress = true;
    deferred_reg.scheduleTask (&report_pending_events_task, false /* permanent */);
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::fireMediaDesc_locked (MediaDesc_Audio * const audio_desc,
                                   bool              const set_audio,
                                   MediaDesc_Video * const video_desc,
                                   bool              const set_video)
{
    logD (stream, _this_func,
          "audio_desc 0x", fmt_hex, (UintPtr) audio_desc, " set_audio ", set_audio, " "
          "video_desc 0x", fmt_hex, (UintPtr) video_desc, " set_video ", set_video);
    if (set_audio && audio_desc)
        logD (stream, _this_func, "auido_desc: ", *audio_desc);
    if (set_video && video_desc)
        logD (stream, _this_func, "video_desc: ", *video_desc);

    if (pending_report_in_progress) {
        PendingMediaDesc * const desc_frame = new (std::nothrow) PendingMediaDesc (
                audio_desc, set_audio, video_desc, set_video);
        assert (desc_frame);
        pending_event_list.append (desc_frame);

        if (msg_inform_counter == 0) {
            ++msg_inform_counter;
            mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
            --msg_inform_counter;
        }

        return;
    }

    ++msg_inform_counter;
    assert (pending_event_list.isEmpty());

    frame_saver.processMediaDesc (audio_desc, set_audio, video_desc, set_video);
    {
        InformMediaDesc_Data inform_data (audio_desc, set_audio, video_desc, set_video);
        mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informMediaDesc, &inform_data);
        mt_async mt_unlocks_locks (mutex) state_informer.informAll_locked (stateInformMediaDesc, &inform_data);
    }

    // Reporting pending frames *after* reporting 'mediaDesc', because
    // 'audio_msg' refers to the previous (pre-bind) stream.
    if (pending_report_in_progress && msg_inform_counter == 1) {
        mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
    }
    --msg_inform_counter;
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::fireAudioMessage_locked (AudioMessage * const mt_nonnull audio_msg)
{
    logS (stream, _this_func,
          "pts ", audio_msg->pts_nanosec, ", "
          "dts ", audio_msg->dts_nanosec, ", "
          "frame_type ", audio_msg->frame_type);

    if (pending_report_in_progress) {
        PendingAudioFrame * const audio_frame = new (std::nothrow) PendingAudioFrame (audio_msg);
        assert (audio_frame);
        pending_event_list.append (audio_frame);

        if (msg_inform_counter == 0) {
            ++msg_inform_counter;
            mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
            --msg_inform_counter;
        }

        return;
    }

    ++msg_inform_counter;
    assert (pending_event_list.isEmpty());

    frame_saver.processAudioFrame (audio_msg);
    {
        InformAudioMessage_Data inform_data (audio_msg);
        mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informAudioMessage, &inform_data);
    }

    // Reporting pending frames *after* reporting 'audio_msg', because
    // 'audio_msg' refers to the previous (pre-bind) stream.
    if (pending_report_in_progress && msg_inform_counter == 1) {
        mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
    }
    --msg_inform_counter;
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::fireVideoMessage_locked (VideoMessage * const mt_nonnull video_msg)
{
    logS (stream, _this_func,
          "pts ", video_msg->pts_nanosec, ", "
          "dts ", video_msg->dts_nanosec, ", "
          "frame_type ", video_msg->frame_type);

    if (pending_report_in_progress) {
        PendingVideoFrame * const video_frame = new (std::nothrow) PendingVideoFrame (video_msg);
        assert (video_frame);
        pending_event_list.append (video_frame);

        if (msg_inform_counter == 0) {
            ++msg_inform_counter;
            mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
            --msg_inform_counter;
        }

        return;
    }

    ++msg_inform_counter;
    assert (pending_event_list.isEmpty());

    frame_saver.processVideoFrame (video_msg);
    {
        InformVideoMessage_Data inform_data (video_msg);
        mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informVideoMessage, &inform_data);
    }

    // Reporting pending frames *after* reporting 'video_msg', because
    // 'video_msg' refers to the previous (pre-bind) stream.
    if (pending_report_in_progress && msg_inform_counter == 1) {
        mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
    }
    --msg_inform_counter;
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::fireRtmpDataMessage_locked (MediaMessage * const mt_nonnull msg)
{
    logS (stream, _this_func,
          "pts ", msg->pts_nanosec, ", "
          "dts ", msg->dts_nanosec);

    if (pending_report_in_progress) {
        PendingDataFrame * const data_frame = new (std::nothrow) PendingDataFrame (msg);
        assert (data_frame);
        pending_event_list.append (data_frame);

        if (msg_inform_counter == 0) {
            ++msg_inform_counter;
            mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
            --msg_inform_counter;
        }

        return;
    }

    ++msg_inform_counter;
    assert (pending_event_list.isEmpty());

    {
        InformRtmpDataMessage_Data inform_data (msg);
        mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informRtmpDataMessage, &inform_data);
    }

    // Reporting pending frames *after* reporting 'msg', because
    // 'msg' refers to the previous (pre-bind) stream.
    if (pending_report_in_progress && msg_inform_counter == 1) {
        mt_async mt_unlocks_locks (mutex) reportPendingEvents ();
    }
    --msg_inform_counter;
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::fireRtmpCommandMessage_locked (RtmpConnection * const mt_nonnull conn,
                                            MediaMessage   * const mt_nonnull msg,
                                            ConstMemory      const method_name,
                                            AmfDecoder     * const mt_nonnull amf_decoder)
{
    InformRtmpCommandMessage_Data inform_data (conn, msg, method_name, amf_decoder);
//#warning amf_decoder is stateful, which breaks notification logics
    mt_async mt_unlocks_locks (mutex) event_informer.informAll_locked (informRtmpCommandMessage, &inform_data);
}

namespace {
struct InformNumWatchersChanged_Data
{
    Count num_watchers;

    InformNumWatchersChanged_Data (Count const num_watchers)
        : num_watchers (num_watchers)
    {}
};
}

void
VideoStream::informNumWatchersChanged (Events * const events,
                                       void   * const cb_data,
                                       void   * const _inform_data)
{
    if (events->numWatchersChanged) {
        InformNumWatchersChanged_Data * const inform_data =
                static_cast <InformNumWatchersChanged_Data*> (_inform_data);
        events->numWatchersChanged (inform_data->num_watchers, cb_data);
    }
}

void
VideoStream::stateInformNumWatchersChanged (StateEvents * const state_events,
                                            void        * const cb_data,
                                            void        * const _inform_data)
{
    if (state_events->numWatchersChanged) {
        InformNumWatchersChanged_Data * const inform_data =
                static_cast <InformNumWatchersChanged_Data*> (_inform_data);
        state_events->numWatchersChanged (inform_data->num_watchers, cb_data);
    }
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::fireNumWatchersChanged_locked ()
{
    InformNumWatchersChanged_Data inform_data (num_watchers);
    mt_unlocks_locks (mutex) event_informer.informAll_locked (informNumWatchersChanged, &inform_data);
    mt_unlocks_locks (mutex) state_informer.informAll_locked (stateInformNumWatchersChanged, &inform_data);
}

bool
VideoStream::reportNumWatchersTask (void * const _self)
{
    VideoStream * const self = static_cast <VideoStream*> (_self);
    logD (stream, _func, "stream 0x", fmt_hex, (UintPtr) self);
    self->mutex.lock ();
    mt_async self->fireNumWatchersChanged_locked ();
    self->mutex.unlock ();
    return false /* do not reschedule */;
}

void
VideoStream::notifyDeferred_NumWatchersChanged ()
{
    deferred_reg.scheduleTask (&report_num_watchers_task, false /* permanent */);
}

void
VideoStream::watcherDeletionCallback (void * const _self)
{
    VideoStream * const self = static_cast <VideoStream*> (_self);
    self->minusOneWatcher ();
}

void
VideoStream::plusOneWatcher (Object * const guard_obj)
{
    mutex.lock ();
    mt_unlocks (mutex) plusOneWatcher_locked (guard_obj);
}

mt_unlocks (mutex) void
VideoStream::plusOneWatcher_locked (Object * const guard_obj)
{
    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->plusOneWatcher() *before* changing 'num_watchers'
    // to avoid races.
    for (;;) {
        Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef();
        Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef();

        if (!abind_stream && !vbind_stream)
            break;

        mutex.unlock ();

        if (abind_stream)
            abind_stream->plusOneWatcher ();

        if (vbind_stream)
            vbind_stream->plusOneWatcher ();

        mutex.lock ();

        {
            Ref<VideoStream> const new_abind_stream = abind.weak_bind_stream.getRef();
            Ref<VideoStream> const new_vbind_stream = vbind.weak_bind_stream.getRef();
            if (new_abind_stream.ptr() == abind_stream.ptr() &&
                new_vbind_stream.ptr() == vbind_stream.ptr())
            {
              // Ok, moving on.
                break;
            }
        }

#if 0
        logD (stream, _func, "extra iteration: "
              "abind: 0x", fmt_hex, (UintPtr) abind.weak_bind_stream.getUnsafePtr(), ", "
              "abind_stream: 0x",   (UintPtr) abind_stream.ptr(), ", "
              "vbind: 0x", fmt_hex, (UintPtr) vbind.weak_bind_stream.getUnsafePtr(), ", "
              "vbind_stream: 0x",   (UintPtr) vbind_stream.ptr());
#endif

        mutex.unlock ();

        if (abind_stream)
            abind_stream->minusOneWatcher ();

        if (vbind_stream)
            vbind_stream->minusOneWatcher ();

        mutex.lock ();
    }

    ++num_watchers;
    mutex.unlock ();

    notifyDeferred_NumWatchersChanged ();

    if (guard_obj) {
        guard_obj->addDeletionCallback (
                CbDesc<Object::DeletionCallback> (watcherDeletionCallback,
                                                  this /* cb_data */,
                                                  this /* guard_obj */));
    }
}

void
VideoStream::minusOneWatcher ()
{
    mutex.lock ();
    mt_unlocks (mutex) minusOneWatcher_locked ();
}

mt_unlocks (mutex) void
VideoStream::minusOneWatcher_locked ()
{
    assert (num_watchers > 0);
    --num_watchers;

    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->minusOneWatcher() *after* changing 'num_watchers'
    // to avoid races.
    Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef ();
    Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef ();
    mutex.unlock ();

    if (abind_stream ||
        vbind_stream)
    {
        if (abind_stream)
            abind_stream->minusOneWatcher ();

        if (vbind_stream)
            vbind_stream->minusOneWatcher ();
    }

    notifyDeferred_NumWatchersChanged ();
}

void
VideoStream::plusWatchers (Count const delta)
{
    if (delta == 0)
        return;

    mutex.lock ();
    mt_unlocks (mutex) plusWatchers_locked (delta);
}

mt_unlocks (mutex) void
VideoStream::plusWatchers_locked (Count const delta)
{
    if (delta == 0) {
        mutex.unlock ();
        return;
    }

    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->plusOneWatcher() *before* changing 'num_watchers'
    // to avoid races.
    for (;;) {
        Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef();
        Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef();

        if (!abind_stream && !vbind_stream)
            break;

        mutex.unlock ();

        if (abind_stream)
            abind_stream->plusWatchers (delta);

        if (vbind_stream)
            vbind_stream->plusWatchers (delta);

        mutex.lock ();

        {
            Ref<VideoStream> const new_abind_stream = abind.weak_bind_stream.getRef();
            Ref<VideoStream> const new_vbind_stream = vbind.weak_bind_stream.getRef();
            if (new_abind_stream.ptr() == abind_stream.ptr() &&
                new_vbind_stream.ptr() == vbind_stream.ptr())
            {
              // Ok, moving on.
                break;
            }
        }

        mutex.unlock ();

        if (abind_stream)
            abind_stream->minusWatchers (delta);

        if (vbind_stream)
            vbind_stream->minusWatchers (delta);

        mutex.lock ();
    }

    num_watchers += delta;
    mutex.unlock ();

    notifyDeferred_NumWatchersChanged ();
}

void
VideoStream::minusWatchers (Count const delta)
{
    if (delta == 0)
        return;

    mutex.lock ();
    mt_unlocks (mutex) minusWatchers_locked (delta);
}

mt_unlocks (mutex) void
VideoStream::minusWatchers_locked (Count const delta)
{
    if (delta == 0) {
        mutex.unlock ();
        return;
    }

    assert (num_watchers >= delta);
    num_watchers -= delta;

    // Keep in mind that bindToStream() can be called at any moment.
    // Calling bind_stream->minusWatchers() *after* changing 'num_watchers'
    // to avoid races.
    Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef ();
    Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef ();
    mutex.unlock ();

    if (abind_stream || vbind_stream) {
        if (abind_stream)
            abind_stream->minusWatchers (delta);

        if (vbind_stream)
            vbind_stream->minusWatchers (delta);
    }

    notifyDeferred_NumWatchersChanged ();
}

mt_mutex (mutex) void
VideoStream::bind_messageBegin (BindInfo     * const mt_nonnull bind_info,
                                MediaMessage * const mt_nonnull msg)
{
    if (!msgHasTimestamp (msg, (bind_info == &abind)))
        return;

    if (!bind_info->got_timestamp_offs) {
        bind_info->got_timestamp_offs = true;
        bind_info->timestamp_offs = stream_timestamp_nanosec - msg->dts_nanosec;

        if (abind.weak_bind_stream.getShadowPtr() == vbind.weak_bind_stream.getShadowPtr()) {
            if (bind_info == &abind) {
                assert (!vbind.got_timestamp_offs);
                vbind.got_timestamp_offs = true;
                vbind.timestamp_offs = abind.timestamp_offs;
            } else {
                assert (!abind.got_timestamp_offs);
                abind.got_timestamp_offs = true;
                abind.timestamp_offs = vbind.timestamp_offs;
            }
        }
    }

    stream_timestamp_nanosec = msg->dts_nanosec + bind_info->timestamp_offs;
}

namespace {
struct BindFrameHandlerData
{
    VideoStream *self;
    bool report_audio;
    bool report_video;
};
}

FrameSaver::FrameHandler const VideoStream::bind_frame_handler = {
    bind_savedAudioFrame,
    bind_savedVideoFrame,
    bind_savedMediaDesc
};

mt_unlocks_locks (mutex) Result
VideoStream::bind_savedAudioFrame (AudioMessage * const mt_nonnull audio_msg,
                                   void         * const _data)
{
    BindFrameHandlerData * const data = static_cast <BindFrameHandlerData*> (_data);
    VideoStream * const self = data->self;

    logS (stream, _self_func, audio_msg->frame_type);

    if (!data->report_audio) {
        logS (stream, _self_func, "ignoring");
        return Result::Success;
    }

    PendingAudioFrame * const audio_frame = new (std::nothrow) PendingAudioFrame (audio_msg);
    assert (audio_frame);
    audio_frame->audio_msg.pts_nanosec += (self->stream_timestamp_nanosec - audio_frame->audio_msg.dts_nanosec);
    audio_frame->audio_msg.dts_nanosec = self->stream_timestamp_nanosec;
    self->pending_event_list.append (audio_frame);

    return Result::Success;
}

mt_unlocks_locks (mutex) Result
VideoStream::bind_savedVideoFrame (VideoMessage * const mt_nonnull video_msg,
                                   void         * const _data)
{
    BindFrameHandlerData * const data = static_cast <BindFrameHandlerData*> (_data);
    VideoStream * const self = data->self;

    logS (stream, _self_func, video_msg->frame_type);

    if (!data->report_video) {
        logS (stream, _self_func, "ignoring");
        return Result::Success;
    }

    PendingVideoFrame * const video_frame = new (std::nothrow) PendingVideoFrame (video_msg);
    assert (video_frame);
    video_frame->video_msg.pts_nanosec += (self->stream_timestamp_nanosec - video_frame->video_msg.dts_nanosec);
    video_frame->video_msg.dts_nanosec = self->stream_timestamp_nanosec;
    self->pending_event_list.append (video_frame);

    return Result::Success;
}

mt_unlocks_locks (mutex) Result
VideoStream::bind_savedMediaDesc (MediaDesc_Audio * audio_desc,
                                  bool              set_audio,
                                  MediaDesc_Video * video_desc,
                                  bool              set_video,
                                  void            * const _data)
{
    BindFrameHandlerData * const data = static_cast <BindFrameHandlerData*> (_data);
    VideoStream * const self = data->self;

    logS (stream, _self_func_);

    if (!data->report_audio) {
        set_audio = false;
        audio_desc = NULL;
    }

    if (!data->report_video) {
        set_video = false;
        video_desc = NULL;
    }

    if (data->report_audio || data->report_video) {
        PendingMediaDesc * const desc_frame = new (std::nothrow) PendingMediaDesc (
                audio_desc, set_audio, video_desc, set_video);
        assert (desc_frame);
        self->pending_event_list.append (desc_frame);
    }

    return Result::Success;
}

VideoStream::Events const VideoStream::abind_events = {
    bind_audioMessage,
    NULL /* videoMessage */,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
//    bind_playbackStart,
//    bind_playbackStop,
    bind_closed,
    NULL /* numWatchersChanged */,
    bind_mediaDesc
};

VideoStream::Events const VideoStream::vbind_events = {
    NULL /* audioMessage */,
    bind_videoMessage,
//#warning TODO For audio streams, bind too.
    bind_rtmpDataMessage,
    NULL /* rtmpCommandMessage */,
//    bind_playbackStart,
//    bind_playbackStop,
    bind_closed,
    NULL /* numWatchersChanged */,
    bind_mediaDesc
};

void
VideoStream::bind_audioMessage (AudioMessage * const mt_nonnull audio_msg,
                                void         * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    self->mutex.lock ();
    if (self->abind.cur_bind_ticket == bind_ticket) {
        self->bind_messageBegin (&self->abind, audio_msg);

        AudioMessage tmp_audio_msg = *audio_msg;
        if (self->abind.got_timestamp_offs) {
            tmp_audio_msg.pts_nanosec += self->abind.timestamp_offs;
            tmp_audio_msg.dts_nanosec += self->abind.timestamp_offs;
        } else {
            tmp_audio_msg.pts_nanosec += (self->stream_timestamp_nanosec - tmp_audio_msg.dts_nanosec);
            tmp_audio_msg.dts_nanosec = self->stream_timestamp_nanosec;
        }

        logS (stream, _func,
              "pts ", tmp_audio_msg.pts_nanosec, ", "
              "dts ", tmp_audio_msg.dts_nanosec, ", "
              "offs ", self->abind.timestamp_offs);

        mt_unlocks_locks (mutex) self->fireAudioMessage_locked (&tmp_audio_msg);
    }
    self->mutex.unlock ();
}

void
VideoStream::bind_videoMessage (VideoMessage * const mt_nonnull video_msg,
                                void         * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    self->mutex.lock ();
    if (self->vbind.cur_bind_ticket == bind_ticket) {
        self->bind_messageBegin (&self->vbind, video_msg);

        VideoMessage tmp_video_msg = *video_msg;
        if (self->vbind.got_timestamp_offs) {
            tmp_video_msg.pts_nanosec += self->vbind.timestamp_offs;
            tmp_video_msg.dts_nanosec += self->vbind.timestamp_offs;
        } else {
            tmp_video_msg.pts_nanosec += (self->stream_timestamp_nanosec - tmp_video_msg.dts_nanosec);
            tmp_video_msg.dts_nanosec = self->stream_timestamp_nanosec;
        }

        logS (stream, _func,
              "pts ", tmp_video_msg.pts_nanosec, ", "
              "dts ", tmp_video_msg.dts_nanosec, ", "
              "offs ", self->vbind.timestamp_offs);

        mt_async mt_unlocks_locks (mutex) self->fireVideoMessage_locked (&tmp_video_msg);
    }
    self->mutex.unlock ();
}

void
VideoStream::bind_mediaDesc (MediaDesc_Audio * const audio_desc,
                             bool              const set_audio,
                             MediaDesc_Video * const video_desc,
                             bool              const set_video,
                             void            * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    self->mutex.lock ();

    bool pass_audio = (self->abind.cur_bind_ticket == bind_ticket);
    bool pass_video = (self->vbind.cur_bind_ticket == bind_ticket);

    if (pass_audio && set_audio) {
        MediaDesc_Audio * const cur_audio_desc = self->frame_saver.getMediaDesc_Audio();
        if (audio_desc) {
            if (cur_audio_desc && audio_desc->equals (cur_audio_desc))
                pass_audio = false;
        } else {
            if (!cur_audio_desc)
                pass_audio = false;
        }
    }

    if (pass_video && set_video) {
        MediaDesc_Video * const cur_video_desc = self->frame_saver.getMediaDesc_Video();
        if (video_desc) {
            if (cur_video_desc && video_desc->equals (cur_video_desc))
                pass_video = false;
        } else {
            if (!cur_video_desc)
                pass_video = false;
        }
    }

    if (pass_audio || pass_video) {
        mt_unlocks_locks (mutex) self->fireMediaDesc_locked (pass_audio ? audio_desc : NULL,
                                                             pass_audio ? set_audio  : false,
                                                             pass_video ? video_desc : NULL,
                                                             pass_video ? set_video  : false);
    }

    self->mutex.unlock ();
}

void
VideoStream::bind_rtmpDataMessage (MediaMessage * const mt_nonnull msg,
                                   void         * const mt_nonnull _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    logS (stream, _self_func_);

    self->mutex.lock ();
    if (self->vbind.cur_bind_ticket == bind_ticket) {
        mt_async mt_unlocks_locks (mutex) self->fireRtmpDataMessage_locked (msg);
    } else {
        logS (stream, _self_func, "bind_ticket mismatch");
    }
    self->mutex.unlock ();
}

void
VideoStream::bind_playbackStart (void * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    logD (stream, _self_func_);

    self->mutex.lock ();

    if (!bind_ticket->bind_layer->valid) {
        self->mutex.unlock ();
        logD (stream, _self_func, "invalid bind layer");
        return;
    }

    if (self->cur_bind_el) {
        BindLayer * const cur_bind_layer = self->cur_bind_el->data;
        if (bind_ticket->bind_layer == cur_bind_layer) {
            self->mutex.unlock ();
            logD (stream, _self_func, "same bind layer");
            return;
        }

        if (bind_ticket->bind_layer->priority <= cur_bind_layer->priority) {
            self->mutex.unlock ();
            logD (stream, _self_func, "same or lower bind priority");
            return;
        }
    }

    Ref<VideoStream> bind_audio_stream = bind_ticket->bind_layer->weak_audio_stream.getRef ();
    Ref<VideoStream> bind_video_stream = bind_ticket->bind_layer->weak_video_stream.getRef ();
    if ((bind_ticket->bind_layer->bind_audio && !bind_audio_stream) ||
        (bind_ticket->bind_layer->bind_video && !bind_video_stream))
    {
        self->mutex.unlock ();
        logD (stream, _self_func, "bind stream gone");
        return;
    }

    self->doBindToStream (bind_audio_stream,
                          bind_video_stream,
                          bind_ticket->bind_layer->bind_audio,
                          bind_ticket->bind_layer->bind_video);

    self->cur_bind_el = bind_ticket->bind_layer->list_el;

    self->mutex.unlock ();
}

void
VideoStream::bind_playbackStop (void * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    logD (stream, _self_func_);

    self->doBindPlaybackStop (bind_ticket);
}

void
VideoStream::bind_closed (void * const _bind_ticket)
{
    BindTicket * const bind_ticket = static_cast <BindTicket*> (_bind_ticket);
    VideoStream * const self = bind_ticket->video_stream;

    logD (stream, _self_func_);

    self->mutex.lock ();

    if (self->abind.cur_bind_ticket == bind_ticket
        && self->abind.propagate_close)
    {
        mt_async self->close_locked ();
    } else
    if (self->vbind.cur_bind_ticket == bind_ticket
        && self->vbind.propagate_close)
    {
        mt_async self->close_locked ();
    }

    self->mutex.unlock ();

    // TODO Necessary to call after closed? (if propagate_close is true)
    self->doBindPlaybackStop (bind_ticket);
}

void
VideoStream::doBindPlaybackStop (BindTicket * const mt_nonnull /* bind_ticket */)
{
//#warning TODO
}

mt_mutex (mutex) void
VideoStream::doBindToStream (VideoStream * const /* bind_audio_stream */,
                             VideoStream * const /* bind_video_stream */,
                             bool          const /* bind_audio */,
                             bool          const /* bind_video */)
{
//#warning TODO
}

bool
VideoStream::closeTask (void * const _self)
{
    VideoStream * const self = static_cast <VideoStream*> (_self);
    self->close ();
    return false /* do not reschedule */;
}

// TODO empty pending_event_list on bindToStream() - carefully, don't strip events.
void
VideoStream::bindToStream (VideoStream * const bind_audio_stream,
                           VideoStream * const bind_video_stream,
                           bool         bind_audio,
                           bool         bind_video,
                           bool          const propagate_close_audio,
                           bool          const propagate_close_video)
{
  // Note: Binding two streams to one another doesn't make _any_ sense (!)
  //   => it *will* break anyway in this case
  //   => we can safely assume that no two streams will cause lock inversion when binding.

    if (!bind_video && !bind_audio)
        return;

    // Cannot bind to self.
    assert (bind_audio_stream != this);
    assert (bind_video_stream != this);

    mutex.lock ();

    if (bind_audio)
        abind.propagate_close = propagate_close_audio;

    if (bind_video)
        vbind.propagate_close = propagate_close_video;

    Ref<VideoStream> const prv_abind_stream = abind.weak_bind_stream.getRef();
    if (prv_abind_stream == bind_audio_stream) {
      // No-op if re-binding to the same stream.
        bind_audio = false;
    }

    Ref<VideoStream> const prv_vbind_stream = vbind.weak_bind_stream.getRef();
    if (prv_vbind_stream == bind_video_stream) {
      // No-op if re-binding to the same stream.
        bind_video = false;
    }

    if (!bind_audio && !bind_video) {
        mutex.unlock ();
        return;
    }

    GenericInformer::SubscriptionKey const prv_abind_sbn = abind.bind_sbn;
    GenericInformer::SubscriptionKey const prv_vbind_sbn = vbind.bind_sbn;
    Count const prv_num_watchers = num_watchers;

    Count abind_plus_watchers = 0;
    Count vbind_plus_watchers = 0;

    if (bind_audio && bind_video && bind_audio_stream == bind_video_stream) {
        {
            abind.timestamp_offs = 0;
            abind.got_timestamp_offs = false;
            abind.weak_bind_stream = bind_audio_stream;

            abind.cur_bind_ticket = grab (new (std::nothrow) BindTicket);
            abind.cur_bind_ticket->video_stream = this;
        }

        {
            vbind.timestamp_offs = 0;
            vbind.got_timestamp_offs = false;
            vbind.weak_bind_stream = bind_video_stream;

            vbind.cur_bind_ticket = grab (new (std::nothrow) BindTicket);
            vbind.cur_bind_ticket->video_stream = this;
        }

        if (VideoStream * const bind_stream = bind_audio_stream /* == bind_video_stream */) {
            bind_stream->mutex.lock ();

            if ((propagate_close_audio || propagate_close_video)
                && bind_stream->isClosed_locked())
            {
                deferred_reg.scheduleTask (&close_task, false /* permanent */);
            }

            {
                BindFrameHandlerData bind_handler_data;
                bind_handler_data.self = this;
                bind_handler_data.report_audio = true;
                bind_handler_data.report_video = true;

                bind_stream->getFrameSaver()->reportSavedFrames (&bind_frame_handler, &bind_handler_data);
            }

            abind.bind_sbn = bind_stream->getEventInformer()->subscribe_locked (
                    CbDesc<Events> (&abind_events, abind.cur_bind_ticket, this, abind.cur_bind_ticket));

            vbind.bind_sbn = bind_stream->getEventInformer()->subscribe_locked (
                    CbDesc<Events> (&vbind_events, vbind.cur_bind_ticket, this, vbind.cur_bind_ticket));

            bind_stream->mutex.unlock ();

            // TODO Don't add/subtract watchers twice for the same stream.
            abind_plus_watchers = num_watchers;
            vbind_plus_watchers = num_watchers;
        } else {
            abind.bind_sbn = NULL;
            vbind.bind_sbn = NULL;
        }
    } else {
        if (bind_audio) {
            if (bind_audio_stream) {
                if (!bind_video_stream && prv_vbind_stream == bind_audio_stream) {
                    abind.timestamp_offs = vbind.timestamp_offs;
                    abind.got_timestamp_offs = vbind.got_timestamp_offs;
                } else {
                    abind.timestamp_offs = 0;
                    abind.got_timestamp_offs = false;
                }

                abind.cur_bind_ticket = grab (new (std::nothrow) BindTicket);
                abind.cur_bind_ticket->video_stream = this;

                abind.weak_bind_stream = bind_audio_stream;

                bind_audio_stream->mutex.lock ();

                if (propagate_close_audio && bind_audio_stream->isClosed_locked())
                    deferred_reg.scheduleTask (&close_task, false /* permanent */);

                {
                    BindFrameHandlerData bind_handler_data;
                    bind_handler_data.self = this;
                    bind_handler_data.report_audio = true;
                    bind_handler_data.report_video = false;

                    bind_audio_stream->getFrameSaver()->reportSavedFrames (&bind_frame_handler, &bind_handler_data);
                }

                abind.bind_sbn = bind_audio_stream->getEventInformer()->subscribe_locked (
                        CbDesc<Events> (&abind_events, abind.cur_bind_ticket, this, abind.cur_bind_ticket));

                bind_audio_stream->mutex.unlock ();

                abind_plus_watchers = num_watchers;
            } else {
                abind.reset ();
            }
        }

        if (bind_video) {
            if (bind_video_stream) {
                if (!bind_audio_stream && prv_abind_stream == bind_video_stream) {
                    vbind.timestamp_offs = abind.timestamp_offs;
                    vbind.got_timestamp_offs = abind.got_timestamp_offs;
                } else {
                    vbind.timestamp_offs = 0;
                    vbind.got_timestamp_offs = false;
                }

                vbind.cur_bind_ticket = grab (new (std::nothrow) BindTicket);
                vbind.cur_bind_ticket->video_stream = this;

                vbind.weak_bind_stream = bind_video_stream;

                bind_video_stream->mutex.lock ();

                if (propagate_close_video && bind_video_stream->isClosed_locked())
                    deferred_reg.scheduleTask (&close_task, false /* permanent */);

                {
                    BindFrameHandlerData bind_handler_data;
                    bind_handler_data.self = this;
                    bind_handler_data.report_audio = false;
                    bind_handler_data.report_video = true;

                    bind_video_stream->getFrameSaver()->reportSavedFrames (&bind_frame_handler, &bind_handler_data);
                }

                vbind.bind_sbn = bind_video_stream->getEventInformer()->subscribe_locked (
                        CbDesc<Events> (&vbind_events, vbind.cur_bind_ticket, this, vbind.cur_bind_ticket));

                bind_video_stream->mutex.unlock ();

                vbind_plus_watchers = num_watchers;
            } else {
                vbind.reset ();
            }
        }
    }

    reportPendingEvents_deferred ();

    if (abind_plus_watchers) {
        assert (bind_audio_stream);
        bind_audio_stream->plusWatchers (abind_plus_watchers);
    }

    if (vbind_plus_watchers) {
        assert (bind_video_stream);
        bind_video_stream->plusWatchers (vbind_plus_watchers);
    }

    mutex.unlock ();

    if (bind_audio && prv_abind_stream) {
        prv_abind_stream->lock ();
        if (prv_abind_sbn) {
            prv_abind_stream->getEventInformer()->unsubscribe_locked (prv_abind_sbn);
        }
        mt_unlocks (prv_abind_stream) prv_abind_stream->minusWatchers_locked (prv_num_watchers);
    }

    // TODO Don't hurry to unsubscribe (keyframe awaiting logics for instant transition).
    if (bind_video && prv_vbind_stream) {
        prv_vbind_stream->lock ();
        if (prv_vbind_sbn) {
            prv_vbind_stream->getEventInformer()->unsubscribe_locked (prv_vbind_sbn);
        }
        mt_unlocks (prv_vbind_stream) prv_vbind_stream->minusWatchers_locked (prv_num_watchers);
    }
}

mt_async void
VideoStream::close_locked ()
{
    is_closed = true;
    mt_unlocks_locks (mutex) event_informer.informAll_locked (informClosed, NULL /* inform_data */);
    mt_unlocks_locks (mutex) state_informer.informAll_locked (stateInformClosed, NULL /* inform_data */);
}

mt_async void
VideoStream::close ()
{
    mutex.lock ();
    mt_async close_locked ();
    mutex.unlock ();
}

namespace {
struct StateInformPublishingStatusChanged_Data
{
    bool publishing_in_progress;
};
}

void
VideoStream::stateInformPublishingStatusChanged (StateEvents * const state_events,
                                                 void        * const cb_data,
                                                 void        * const _inform_data)
{
    StateInformPublishingStatusChanged_Data * const inform_data =
            static_cast <StateInformPublishingStatusChanged_Data*> (_inform_data);

    if (state_events->publishingStatusChanged)
        state_events->publishingStatusChanged (inform_data->publishing_in_progress, cb_data);
}

mt_async mt_unlocks_locks (mutex) void
VideoStream::firePublishingStatusChanged_locked (bool const in_progress)
{
    StateInformPublishingStatusChanged_Data data;
    data.publishing_in_progress = in_progress;

    mt_async mt_unlocks_locks (mutex) state_informer.informAll_locked (stateInformPublishingStatusChanged, &data);
}

void
VideoStream::setPublishingInProgress (bool const in_progress)
{
    mutex.lock ();
    if (publishing_in_progress == in_progress) {
        mutex.unlock ();
        logD (stream, _this_func, "redundant invocation");
        return;
    }

    PendingEvent * const event = new (std::nothrow) PendingEvent (
            in_progress ? PendingEvent::t_Event_BeginPublishing : PendingEvent::t_Event_EndPublishing);
    assert (event);
    pending_event_list.append (event);

    reportPendingEvents_deferred ();
    mutex.unlock ();
}

VideoStream::VideoStream (EmbedContainer    * const embed_container,
                          DeferredProcessor * const mt_nonnull deferred_processor,
                          bool                const publishing_in_progress,
                          bool                const enable_frame_saver,
                          bool                const send_saved_video_frames)
    : MediaSource                (embed_container, MediaSource::Type::MediaStream),
      frame_saver                (enable_frame_saver, send_saved_video_frames),
      event_informer             (this, &mutex),
      state_informer             (this, &mutex),
      num_watchers               (0),
      cur_bind_el                (NULL),
      stream_timestamp_nanosec   (1000000000),
      pending_report_in_progress (false),
      msg_inform_counter         (0),
      is_closed                  (false),
      publishing_in_progress     (publishing_in_progress)
{
    report_num_watchers_task.cb   = CbDesc<DeferredProcessor::TaskCallback> (reportNumWatchersTask,   this, this);
    report_pending_events_task.cb = CbDesc<DeferredProcessor::TaskCallback> (reportPendingEventsTask, this, this);
    close_task.cb                 = CbDesc<DeferredProcessor::TaskCallback> (closeTask,               this, this);

    deferred_reg.setDeferredProcessor (deferred_processor);
}

VideoStream::~VideoStream ()
{
    // This lock ensures data consistency for 'frame_saver's destructor.
    // TODO ^^^ Does it? A single mutex lock/unlock pair does not (ideally) constitute
    //      a full memory barrier.
    mutex.lock ();

    // Here's why we do not to fire closed() event here:
    //      * It's common to keep Ref<VideoStream> and wait for a 'closed' event
    //        Firing 'closed' event from dtor doesn't help in such cases,
    //        hence missing explicit close() call is always a bug;
    //      * Making mt_async calls from dtors may be a bad idea.
    // mt_async close_locked ();

    {
        Ref<VideoStream> const abind_stream = abind.weak_bind_stream.getRef ();
        Ref<VideoStream> const vbind_stream = vbind.weak_bind_stream.getRef ();
        if (abind_stream ||
            vbind_stream)
        {
            Count const tmp_num_watchers = num_watchers;
            mutex.unlock ();

            if (abind_stream)
                abind_stream->minusWatchers (tmp_num_watchers);

            if (vbind_stream)
                vbind_stream->minusWatchers (tmp_num_watchers);

            mutex.lock ();
        }
    }

    {
        PendingEventList::iterator iter (pending_event_list);
        while (!iter.done()) {
            PendingEvent * const pending_event = iter.next ();
            delete pending_event;
        }
    }

    mutex.unlock ();
}

}

