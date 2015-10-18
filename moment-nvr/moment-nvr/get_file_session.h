/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__GET_FILE_SESSION__H__
#define MOMENT_NVR__GET_FILE_SESSION__H__


#include <moment-nvr/media_reader.h>


namespace MomentNvr {

class GetFileSession : public Object
{
  private:
    StateMutex mutex;

  public:
    struct Frontend {
//#warning TODO implement 'done' cb calls
        void (*done) (Result  res,
                      void   *cb_data);
    };

  private:
    enum SessionState {
        SessionState_Header,
        SessionState_Data,
        SessionState_Complete
    };

    mt_const Ref<MomentServer> moment;
    mt_const Ref<PagePool> page_pool;
    mt_const Ref<Sender>   sender;
    mt_const Cb<Frontend> frontend;

    mt_const Time start_unixtime_sec;
    mt_const Time duration_sec;

    mt_const bool octet_stream_mime;

    mt_const bool req_is_keepalive;
    mt_const IpAddress req_client_addr;
    mt_const StRef<String> req_request_line;

    // Bytes per second. 0 means no limit.
    mt_const Uint64 first_burst_size;
    mt_const Uint64 bps_limit;
    mt_sync_domain (readTask) Time transfer_start_time_millisec;
    mt_sync_domain (readTask) Uint64 bytes_transferred;

    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const ServerThreadContext *reader_thread_ctx;

    Embed<MediaReader> media_reader;
    mt_sync_domain (readTask) Mp4Muxer mp4_muxer;

    mt_sync_domain (readTask) SessionState session_state;
    mt_sync_domain (readTask) bool first_data_read;

    mt_sync_domain (readTask) bool got_last_audio_ts;
    mt_sync_domain (readTask) Time last_audio_ts_nanosec;
    mt_sync_domain (readTask) bool got_last_video_ts;
    mt_sync_domain (readTask) Time last_video_ts_nanosec;

    mt_sync_domain (readTask) Uint64 total_num_frames;
    mt_sync_domain (readTask) Uint64 pass2_num_frames;

    DeferredProcessor::Task read_task;
    DeferredProcessor::Task sender_closed_task;
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) bool started;

    mt_mutex (mutex) Timers::TimerKey bps_limit_timer;

    mt_iface (MediaReader::ReadFrameBackend)
      static MediaReader::ReadFrameBackend const read_frame_backend;

      mt_sync_domain (readTask) static
              MediaReader::ReadFrameResult audioFrame (AudioMessage * mt_nonnull msg,
                                                       void         *_self);

      mt_sync_domain (readTask) static
              MediaReader::ReadFrameResult videoFrame (VideoMessage * mt_nonnull msg,
                                                       void         *_self);
    mt_iface_end

    mt_sync_domain (readTask)
            MediaReader::ReadFrameResult doSendFrame (MediaMessage * mt_nonnull msg,
                                                      Time          last_ts_nanosec);

    static mt_sync_domain (readTask) bool senderClosedTask (void *_self);

    static mt_sync_domain (readTask) bool readTask (void *_self);

    static mt_sync_domain (readTask) void bpsLimitTimerTick (void *_self);

    mt_sync_domain (readTask) void doReadData ();

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void senderStateChanged (SenderState  sender_state,
                                      void        *_self);

      static void senderClosed (Exception *exc,
                                void      *_self);
    mt_iface_end 

  public:
    void start ();

    mt_const void init (MomentServer * mt_nonnull moment,
                        HttpRequest  * mt_nonnull req,
                        Sender       * mt_nonnull sender,
                        PagePool     * page_pool,
                        Vfs          * vfs,
                        ConstMemory    stream_name,
                        Time           start_unixtime_sec,
                        Time           duration_sec,
                        bool           octet_stream_mime,
                        CbDesc<Frontend> const &frontend);

     GetFileSession (EmbedContainer *embed_container);
    ~GetFileSession ();
};

}


#endif /* MOMENT_NVR__GET_FILE_SESSION__H__ */

