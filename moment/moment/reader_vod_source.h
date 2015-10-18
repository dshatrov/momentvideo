/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__READER_VOD_SOURCE__H__
#define MOMENT__READER_VOD_SOURCE__H__


#include <moment/vod_source.h>
#include <moment/vod_reader.h>


#define MOMENT__READER_VOD_SOURCE__DEFAULT_MAX_BURST_SIZE 2097152


namespace Moment {

class ReaderVodSource : public VodSource
{
  private:
    StateMutex mutex;

    enum State
    {
        State_Started,
        State_Blocked,
        State_Stopped,
        State_Error
    };

    mt_const StRef<VodReader> vod_reader;

    mt_const Ref<Timers> timers;

    mt_const Size max_burst_size;

    DeferredProcessor::Task read_task;
    DeferredProcessor::Registration deferred_reg;

    // 'frontend' is safe to use without synchronization, because the user is
    // allowed to call setFrontend() only when it's safe (no async operations
    // in progress).
    mt_mutex (mutex) Cb<Frontend> frontend;

    mt_mutex (mutex) bool stop_time_enabled;
    mt_mutex (mutex) Time stop_time_microsec;

    mt_mutex (mutex) State state;
    mt_mutex (mutex) bool stop_request;

    mt_mutex (mutex) bool got_seek_request;
    mt_mutex (mutex) Time seek_request_pos_microsec;

    mt_mutex (mutex) bool   got_set_timelapse;
    mt_mutex (mutex) Time   set_timelapse_frame_interval_millisec;
    mt_mutex (mutex) Uint32 set_timelapse_frames_per_second;

    mt_mutex (mutex) bool got_query_source_info_request;
    mt_mutex (mutex) Cb<QuerySourceInfoCallback> query_source_info_cb;

    void cancelQuerySourceInfo ();

    static bool readTask (void *_self);

  public:
    mt_iface (VodSource)
      Timers* getTimers () { return timers; }

      void setFrontend (CbDesc<VodSource::Frontend> const &frontend)
      {
          mutex.lock ();
          this->frontend = frontend;
          mutex.unlock ();
      }

      void start  (bool _stop_time_enabled,
                   Time _stop_time_microsec);
      void stop   ();

      void block  ();
      void resume ();

      void seek (Time seek_pos_microsec);

      void setTimelapse (Time   timelapse_frame_interval_millisec,
                         Uint32 timelapse_frames_per_second);

      bool querySourceInfo (CbDesc<VodSource::QuerySourceInfoCallback> const &cb,
                            VodSourceInfo * mt_nonnull ret_source_info);
    mt_iface_end

    mt_const void init (DeferredProcessor * mt_nonnull deferred_processor,
                        Timers            * mt_nonnull timers,
                        VodReader         * mt_nonnull vod_reader,
                        Size               max_burst_size = MOMENT__READER_VOD_SOURCE__DEFAULT_MAX_BURST_SIZE);

    ReaderVodSource (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__READER_VOD_SOURCE__H__ */

