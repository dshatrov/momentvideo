/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/av_recorder.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_recorder ("av_recorder", LogLevel::I);
static LogGroup libMary_logGroup_recorder_frames ("av_recorder_frames", LogLevel::I);

Sender::Frontend const AvRecorder::sender_frontend = {
    senderStateChanged,
    senderClosed
};

FrameSaver::FrameHandler const AvRecorder::saved_frame_handler = {
    savedAudioFrame,
    savedVideoFrame,
    NULL /* mediaDesc */
};

Result
AvRecorder::savedAudioFrame (AudioMessage * const mt_nonnull audio_msg,
                             void         * const _self)
{
    AvRecorder * const self = static_cast <AvRecorder*> (_self);

    if (!audio_msg->frame_type.isAudioCodecData())
        return Result::Success;

    // TODO Use codec data from saved MediaDesc instead.
    AudioMessage tmp_audio_msg = *audio_msg;
    tmp_audio_msg.pts_nanosec = 0;
    tmp_audio_msg.dts_nanosec = 0;

    if (!self->muxer->muxAudioMessage (&tmp_audio_msg)) {
        logE (recorder, _func, "muxer->muxAudioMessage() failed (aac seq hdr): ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

Result
AvRecorder::savedVideoFrame (VideoMessage * const mt_nonnull video_msg,
                             void         * const _self)
{
    AvRecorder * const self = static_cast <AvRecorder*> (_self);

    if (!video_msg->frame_type.isVideoCodecData())
        return Result::Success;

    // TODO Use codec data from saved MediaDesc instead.
    VideoMessage tmp_video_msg = *video_msg;
    tmp_video_msg.pts_nanosec = 0;
    tmp_video_msg.dts_nanosec = 0;

    if (!self->muxer->muxVideoMessage (&tmp_video_msg)) {
        logE (recorder, _func, "muxer->muxVideoMessage() failed (metadata): ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

mt_mutex (mutex) void
AvRecorder::muxInitialMessages ()
{
    logD (recorder_frames, _func_);

    if (!video_stream)
	return;

    video_stream->lock ();

    FrameSaver * const frame_saver = video_stream->getFrameSaver ();
    if (!frame_saver)
	goto _return;

    if (!frame_saver->reportSavedFrames (&saved_frame_handler, this)) {
        doStop ();
        goto _return;
    }

_return:
    video_stream->unlock ();
}

mt_mutex (mutex) void
AvRecorder::doStop ()
{
    cur_stream_ticket = NULL;

    if (recording) {
	// Note that muxer->endMuxing() implies recording->sender.closeAfterFlush().
	if (!muxer->endMuxing ())
	    logE (recorder, _func, "muxer->endMuxing() failed: ", exc->toString());

      // The file can't be released at this point, because it is used by
      // deferred sender. The file is released later in senderClosed().

	recording = NULL;
    }
}

void
AvRecorder::senderStateChanged (SenderState   const /* sender_state */,
                                void        * const /* _recording */)
{
  // TODO Start dropping frames when destination is overloaded.
  //      This doesn't matter for local files, because local write operations
  //      always block till completion.
}

void
AvRecorder::senderClosed (Exception * const exc_,
			  void      * const _recording)
{
    Recording * const recording = static_cast <Recording*> (_recording);

    if (exc_)
	logE (recorder, _func, "exception: ", exc_->toString());

    Ref<AvRecorder> const self = recording->weak_av_recorder.getRef ();
    if (!self)
	return;

    recording->mutex.lock ();
    recording->storage_file = NULL;
    recording->mutex.unlock ();
}

VideoStream::Events const AvRecorder::stream_events = {
    streamAudioMessage,
    streamVideoMessage,
    NULL /* rtmpDataMessage */,
    NULL /* rtmpCommandMessage */,
    streamClosed,
    NULL /* numWatchersChanged */,
    NULL /* mediaDesc */
};

void
AvRecorder::streamAudioMessage (AudioMessage * const mt_nonnull msg,
				void         * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    if (!self->recording) {
	self->mutex.unlock ();
	return;
    }

    if (self->total_bytes_recorded >= self->recording_limit) {
	self->doStop ();
	self->mutex.unlock ();
	return;
    }

    if (!self->got_first_frame & msg->frame_type.isAudioData()) {
	logD (recorder_frames, _func, "first frame (audio)");
	self->got_first_frame = true;
	// Note that integer overflows are possible here and that's fine.
	self->first_frame_time = msg->dts_nanosec;
    }
    Time const dts = msg->dts_nanosec - self->first_frame_time;
    logD (recorder_frames, _func, "dts ", dts, ", got_first_frame: ", self->got_first_frame);

    AudioMessage alt_msg = *msg;
    alt_msg.pts_nanosec += dts - msg->dts_nanosec;
    alt_msg.dts_nanosec = dts;

    if (!self->muxer->muxAudioMessage (&alt_msg)) {
	logE (recorder, _func, "muxer->muxAudioMessage() failed: ", exc->toString());
	self->doStop ();
    }

    self->total_bytes_recorded += alt_msg.msg_len;

    self->mutex.unlock ();
}

void
AvRecorder::streamVideoMessage (VideoMessage * const mt_nonnull msg,
				void         * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    if (!self->recording) {
	self->mutex.unlock ();
	return;
    }

    if (self->total_bytes_recorded >= self->recording_limit) {
	self->doStop ();
	self->mutex.unlock ();
	return;
    }

    if (!self->got_first_frame && msg->frame_type.isVideoData()) {
	logD (recorder_frames, _func, "first frame (video)");
	self->got_first_frame = true;
	// Note that integer overflows are possible here and that's fine.
	self->first_frame_time = msg->dts_nanosec;
    }
    Time const dts = msg->dts_nanosec - self->first_frame_time;
    logD (recorder_frames, _func, "dts ", dts, ", got_first_frame: ", self->got_first_frame);

    VideoMessage alt_msg = *msg;
    alt_msg.pts_nanosec += dts - msg->dts_nanosec;
    alt_msg.dts_nanosec = dts;

    if (!self->muxer->muxVideoMessage (&alt_msg)) {
	logE (recorder, _func, "muxer->muxVideoMessage() failed: ", exc->toString());
	self->doStop ();
    }

    self->total_bytes_recorded += alt_msg.msg_len;

    self->mutex.unlock ();
}

void
AvRecorder::streamClosed (void * const _stream_ticket)
{
    StreamTicket * const stream_ticket = static_cast <StreamTicket*> (_stream_ticket);
    AvRecorder * const self = stream_ticket->av_recorder;

    self->mutex.lock ();
    if (self->cur_stream_ticket != stream_ticket) {
	self->mutex.unlock ();
	return;
    }

    self->doStop ();
    self->mutex.unlock ();
}

void
AvRecorder::start (ConstMemory const filename,
                   Uint32      const create_permissions)
{
    logD_ (_this_func, "filename: ", filename);

    mutex.lock ();

    if (recording) {
      // TODO Stop current recording and start a new one.
	logW (recorder, _func, "Already recording");
	mutex.unlock ();
	return;
    }

    got_first_frame = false;
    first_frame_time = 0;

    recording = grab (new (std::nothrow) Recording (NULL /* embed_container */));
    muxer->setSender (recording->sender);

    recording->storage_file = storage->openFile (filename, thread_ctx->getDeferredProcessor(), create_permissions);
    if (!recording->storage_file) {
	logE (recorder, _func, "storage->openFile() failed for filename ",
	      filename, ": ", exc->toString());
	recording = NULL;
	mutex.unlock ();
	return;
    }
    recording->conn = recording->storage_file->getConnection();

    recording->weak_av_recorder = this;

    recording->sender->setConnection (recording->conn);
    recording->sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    recording->sender->setFrontend (
	    CbDesc<Sender::Frontend> (&sender_frontend,
				      recording /* cb_data */,
				      recording /* guard_obj */));

    paused = false;

    if (!muxer->beginMuxing ()) {
	logE_ (_func, "muxer->beginMuxing() failed: ", exc->toString());
	// TODO Fail?
    }

    muxInitialMessages ();

    mutex.unlock ();
}

void
AvRecorder::pause ()
{
    mutex.lock ();
    paused = true;
    // TODO Track keyframes.
//#warning TODO flush sender, or what?
    mutex.unlock ();
}

void
AvRecorder::resume ()
{
    mutex.lock ();
    paused = false;
    mutex.unlock ();
}

void
AvRecorder::stop ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

void
AvRecorder::setVideoStream (VideoStream * const stream)
{
    mutex.lock ();

    this->video_stream = stream;

    cur_stream_ticket = grab (new (std::nothrow) StreamTicket (this));

  // TODO Unsubsribe from the previous stream's events.

    got_first_frame = false;

    // TODO Fix race condition with stream_closed() (What if the stream has just been closed?)
    stream->getEventInformer()->subscribe (
	    CbDesc<VideoStream::Events> (
		    &stream_events,
		    cur_stream_ticket /* cb_data */,
                    this,
		    cur_stream_ticket));

    mutex.unlock ();
}

mt_const void
AvRecorder::init (CbDesc<Frontend> const &frontend,
                  ServerThreadContext * mt_nonnull thread_ctx,
                  Storage             * mt_nonnull storage,
                  AvMuxer             * mt_nonnull muxer,
                  Uint64               recording_limit)
{
    this->frontend   = frontend;
    this->thread_ctx = thread_ctx;
    this->storage    = storage;
    this->muxer      = muxer;
    this->recording_limit = recording_limit;
}

AvRecorder::AvRecorder (EmbedContainer * const embed_container)
    : Object           (embed_container),
      muxer            (NULL),
      recording_limit  (0),
      paused           (false),
      got_first_frame  (false),
      first_frame_time (0),
      total_bytes_recorded (0)
{
}

AvRecorder::~AvRecorder ()
{
    mutex.lock ();
    doStop ();
    mutex.unlock ();
}

}

