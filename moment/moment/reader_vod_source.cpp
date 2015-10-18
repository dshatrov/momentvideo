/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_h264.h>

#include <moment/reader_vod_source.h>


namespace Moment {

static LogGroup libMary_logGroup_reader ("moment.reader_vod_source", LogLevel::S);

mt_unlocks (mutex) void
ReaderVodSource::cancelQuerySourceInfo ()
{
    logD (reader, _this_func_);

    if (got_query_source_info_request) {
        got_query_source_info_request = false;

        Cb<QuerySourceInfoCallback> const cb = query_source_info_cb;
        query_source_info_cb.reset ();
        mutex.unlock ();

        VodSourceInfo null_source_info;
        cb.call_ (&null_source_info);
    } else {
        mutex.unlock ();
    }
}

bool
ReaderVodSource::readTask (void * const _self)
{
    ReaderVodSource * const self = static_cast <ReaderVodSource*> (_self);

    logS (reader, _self_func_);

    self->mutex.lock ();
    if (self->state == State_Error) {
        mt_unlocks (mutex) self->cancelQuerySourceInfo ();
        return false /* do not reschedule */;
    }

    if (self->stop_request) {
        self->state = State_Stopped;
        mt_unlocks (mutex) self->cancelQuerySourceInfo ();

        if (self->frontend) {
            self->frontend.call (self->frontend->endOfSource,
                                 /*(*/
                                     EndReason::Stopped
                                 /*)*/);
        }

        return false /* do not reschedule */;
    }

    if (self->got_seek_request) {
        self->got_seek_request = false;

        Time const seek_pos_microsec = self->seek_request_pos_microsec;
        self->seek_request_pos_microsec = 0;

        self->mutex.unlock ();

        {
            VodReader::SeekResult const res = self->vod_reader->seek (seek_pos_microsec);
            if (res == VodReader::SeekResult::Error) {
                self->mutex.lock ();
                self->state = State_Error;

                logE (reader, _self_func, "seek failed");
                mt_unlocks (mutex) self->cancelQuerySourceInfo ();

                if (self->frontend) {
                    self->frontend.call (self->frontend->endOfSource,
                                         /*(*/
                                             EndReason::Error
                                         /*)*/);
                }

                return false /* do not reschedule */;
            } else
            if (res == VodReader::SeekResult::SeekPastEnd) {
                self->mutex.lock ();
                self->state = State_Stopped;

                logD (reader, _self_func, "seek past end");
                mt_unlocks (mutex) self->cancelQuerySourceInfo ();

                if (self->frontend) {
                    self->frontend.call (self->frontend->endOfSource,
                                         /*(*/
                                             EndReason::NoMoreFrames
                                         /*)*/);
                }

                return false /* do not reschedule */;
            }
            assert (res == VodReader::SeekResult::Normal);
        }

        self->mutex.lock ();
        if (self->got_seek_request) {
            self->mutex.unlock ();
            return true /* reschedule */;
        }
    } // if (self->got_seek_request)

    if (self->got_set_timelapse) {
        self->got_set_timelapse = false;

        Time const timelapse_frame_interval_millisec = self->set_timelapse_frame_interval_millisec;
        Time const timelapse_frames_per_second = self->set_timelapse_frames_per_second;
        self->set_timelapse_frame_interval_millisec = 0;
        self->set_timelapse_frames_per_second = 0;

        self->mutex.unlock ();
        self->vod_reader->setTimelapse (timelapse_frame_interval_millisec,
                                        timelapse_frames_per_second);
        self->mutex.lock ();
    }

    if (self->got_query_source_info_request) {
        self->got_query_source_info_request = false;
        {
            Cb<QuerySourceInfoCallback> const cb = self->query_source_info_cb;
            self->query_source_info_cb.reset ();
            self->mutex.unlock ();

            VodSourceInfo source_info;
            if (!self->vod_reader->getSourceInfo (&source_info)) {
                logE (reader, _self_func, "vod_reader.getSourceInfo() failed");
                VodSourceInfo null_source_info;
                cb.call_ (&null_source_info);
            } else {
                cb.call_ (&source_info);
            }
        }
        self->mutex.lock ();
    }

    if (self->stop_request) {
        self->state = State_Stopped;
        self->mutex.unlock ();

        if (self->frontend) {
            self->frontend.call (self->frontend->endOfSource,
                                 /*(*/
                                     EndReason::Stopped
                                 /*)*/);
        }

        return false /* do not reschedule */;
    }

    Size total_size = 0;
    while (self->state == State_Started) {
        self->mutex.unlock ();

        VodReader::ReadMessage read_msg;
        MediaMessage *msg = NULL;
        VodReader::ReadResult const res = self->vod_reader->readNextFrame (&read_msg,
                                                                           &msg,
                                                                           self->stop_time_enabled,
                                                                           self->stop_time_microsec);
        if (res == VodReader::ReadResult::Error) {
            assert (!msg);

            self->mutex.lock ();
            if (   self->state == State_Error
                || self->state == State_Stopped)
            {
                self->mutex.unlock ();
                return false /* do not reschedule */;
            }
            self->state = State_Error;
            self->mutex.unlock ();

            if (self->frontend) {
                self->frontend.call (self->frontend->endOfSource,
                                     /*(*/
                                         EndReason::Error
                                     /*)*/);
            }

            return false /* do not reschedule */;
        } else
        if (   res == VodReader::ReadResult::NoMoreFrames
            || res == VodReader::ReadResult::StopTimeReached)
        {
            assert (!msg);

            self->mutex.lock ();
            if (   self->state == State_Error
                || self->state == State_Stopped)
            {
                self->mutex.unlock ();
                return false /* do not reschedule */;
            }
            self->state = State_Stopped;
            self->mutex.unlock ();

            EndReason end_reason = EndReason::NoMoreFrames;
            if (res == VodReader::ReadResult::StopTimeReached)
                end_reason = EndReason::StopTimeReached;

            if (self->frontend)
                self->frontend.call (self->frontend->endOfSource, /*(*/ end_reason /*)*/);

            return false /* do not reschedule */;
        }
        assert (res == VodReader::ReadResult::Normal && msg);

        logS (reader, _self_func "msg: ", *msg);
//        if (msg->msg_type == MediaMessage::Type_Video)
//            dumpH264AvcNalUnits (msg->page_list.first, msg->msg_offset, msg->msg_len);

//        PagePool::dumpPages (logs, &msg->page_list, msg->msg_offset);

        total_size += msg->msg_len;

        if (self->frontend) {
            self->frontend.call (self->frontend->mediaMessage, /*(*/ msg /*)*/);
        }
        msg->release ();

        if (total_size >= self->max_burst_size) {
            logS (reader, _self_func "burst limit");
            return true /* reschedule */;
        }

        self->mutex.lock ();
        if (self->stop_request) {
            self->state = State_Stopped;
            self->mutex.unlock ();

            if (self->frontend) {
                self->frontend.call (self->frontend->endOfSource,
                                     /*(*/
                                         EndReason::Stopped
                                     /*)*/);
            }

            return false /* do not reschedule */;
        }

#if 0
// Deprecated
// TODO Return 'true' from frontend->mediaMessage() to block.

        if (self->auto_block && self->state == State_Started) {
            if (   msg->msg_type == MediaMessage::Type_Audio
                || msg->msg_type == MediaMessage::Type_Video)
            {
                self->state = State_Blocked;
            }
        }
#endif
    } // while (self->state == State_Started)

    self->mutex.unlock ();

    return false /* do not reschedule */;
}

void
ReaderVodSource::start (bool const _stop_time_enabled,
                        Time const _stop_time_microsec)
{
    logD (reader, _this_func,
          "stop_time_enabled ", _stop_time_enabled, ", "
          "stop_time_microsec ", _stop_time_microsec);

    mutex.lock ();
    if (state != State_Stopped) {
        mutex.unlock ();
        return;
    }

    stop_time_enabled  = _stop_time_enabled;
    stop_time_microsec = _stop_time_microsec;

    state = State_Started;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
}

void
ReaderVodSource::stop ()
{
    mutex.lock ();
    if (   state == State_Stopped
        || state == State_Error)
    {
        mutex.unlock ();
        return;
    }

    stop_request = true;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
}

void
ReaderVodSource::block ()
{
    mutex.lock ();
    if (state != State_Started) {
        mutex.unlock ();
        return;
    }

    state = State_Blocked;
    mutex.unlock ();
}

void
ReaderVodSource::resume ()
{
    mutex.lock ();
    if (state != State_Blocked) {
        mutex.unlock ();
        return;
    }

    state = State_Started;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
}

void
ReaderVodSource::seek (Time const seek_pos_microsec)
{
    logD (reader, _this_func, "seek_pos_microsec ", seek_pos_microsec);

    mutex.lock ();
    got_seek_request = true;
    seek_request_pos_microsec = seek_pos_microsec;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
}

void
ReaderVodSource::setTimelapse (Time   const timelapse_frame_interval_millisec,
                               Uint32 const timelapse_frames_per_second)
{
    mutex.lock ();
    got_set_timelapse = true;
    set_timelapse_frame_interval_millisec = timelapse_frame_interval_millisec;
    set_timelapse_frames_per_second = timelapse_frames_per_second;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
}

bool
ReaderVodSource::querySourceInfo (CbDesc<VodSource::QuerySourceInfoCallback> const &cb,
                                  VodSourceInfo * const mt_nonnull /* ret_source_info */)
{
    logD (reader, _this_func_);

    mutex.lock ();
    got_query_source_info_request = true;
    query_source_info_cb = cb;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
    return false;
}

mt_const void
ReaderVodSource::init (DeferredProcessor * const mt_nonnull deferred_processor,
                       Timers            * const mt_nonnull timers,
                       VodReader         * const mt_nonnull vod_reader,
                       Size                const max_burst_size)
{
    this->vod_reader = vod_reader;
    this->timers = timers;
    this->max_burst_size = max_burst_size;

    deferred_reg.setDeferredProcessor (deferred_processor);
}

ReaderVodSource::ReaderVodSource (EmbedContainer * const embed_container)
    : VodSource                     (embed_container),
      stop_time_enabled             (false),
      stop_time_microsec            (0),
      state                         (State_Stopped),
      stop_request                  (false),
      got_seek_request              (false),
      seek_request_pos_microsec     (0),
      got_set_timelapse             (false),
      set_timelapse_frame_interval_millisec (0),
      set_timelapse_frames_per_second (0),
      got_query_source_info_request (false)
{
    read_task.cb = CbDesc<DeferredProcessor::TaskCallback> (readTask, this, this);
}

}

