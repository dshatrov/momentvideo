/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__MEDIA_RECORDER__H__
#define MOMENT_NVR__MEDIA_RECORDER__H__


#include <moment-nvr/types.h>
#include <moment-nvr/naming_scheme.h>


namespace MomentNvr {

class MediaRecorder : public Object
{
  private:
    StateMutex mutex;

  private:
    struct Recording : public Object
    {
        WeakRef<MediaRecorder> weak_media_recorder;

        Embed<DeferredConnectionSender> vdat_sender;
        mt_mutex (MediaRecorder::mutex) StRef<Vfs::VfsFile> vdat_file;
        Embed<FileConnection> vdat_conn;

        Embed<DeferredConnectionSender> idx_sender;
        mt_mutex (MediaRecorder::mutex) StRef<Vfs::VfsFile> idx_file;
        Embed<FileConnection> idx_conn;

        mt_mutex (MediaRecorder::mutex) Size cur_data_offset;

        Recording (EmbedContainer * const embed_container)
            : Object      (embed_container),
              vdat_sender (this /* embed_container */),
              vdat_conn   (this /* embed_container */),
              idx_sender  (this /* embed_container */),
              idx_conn    (this /* embed_container */),
              cur_data_offset (0)
        {}
    };

    class PrewriteQueue_name;

    struct PrewriteQueueEntry : public IntrusiveListElement<PrewriteQueue_name>
    {
        enum EntryType {
            EntryType_Audio,
            EntryType_Video
        };

        EntryType entry_type;

        AudioMessage audio_msg;
        VideoMessage video_msg;

        Time getDtsNanosec () const
        {
            if (entry_type == EntryType_Audio)
                return audio_msg.dts_nanosec;

            return video_msg.dts_nanosec;
        }

        ~PrewriteQueueEntry ()
        {
            if (audio_msg.page_pool)
                audio_msg.page_pool->msgUnref (audio_msg.page_list.first);

            if (video_msg.page_pool)
                video_msg.page_pool->msgUnref (video_msg.page_list.first);
        }
    };

    typedef IntrusiveList< PrewriteQueueEntry,
                           PrewriteQueue_name,
                           DeleteAction<PrewriteQueueEntry> > PrewriteQueue;

    mt_const Ref<PagePool> page_pool;
    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const StRef<Vfs> vfs;

    mt_const Ref<NamingScheme> naming_scheme;
    mt_const StRef<String> channel_name;

    mt_const Time  prewrite_nanosec;
    mt_const Count prewrite_num_frames_limit;
    mt_const Time  postwrite_nanosec;
    mt_const Count postwrite_num_frames_limit;

    struct StreamTicket : public Referenced
        { MediaRecorder *media_recorder; };

    mt_mutex (mutex) Ref<StreamTicket> cur_stream_ticket;
    mt_mutex (mutex) Ref<VideoStream> cur_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey stream_sbn;
    mt_mutex (mutex) bool got_unixtime_offset;
    mt_mutex (mutex) Time unixtime_offset_nanosec;
    mt_mutex (mutex) Time prv_unixtime_dts_nanosec;

    mt_mutex (mutex) bool got_pending_aac_seq_hdr;
    mt_mutex (mutex) AudioMessage pending_aac_seq_hdr;

    mt_mutex (mutex) bool got_pending_avc_seq_hdr;
    mt_mutex (mutex) VideoMessage pending_avc_seq_hdr;

    mt_mutex (mutex) bool got_avc_seq_hdr;

    mt_mutex (mutex) Time next_idx_unixtime_nanosec;

    mt_mutex (mutex) Ref<Recording> recording;
    mt_mutex (mutex) Time next_file_unixtime_nanosec;

    mt_mutex (mutex) bool postwrite_active;
    mt_mutex (mutex) bool got_postwrite_start_ts;
    mt_mutex (mutex) Time postwrite_start_ts_nanosec;
    mt_mutex (mutex) Count postwrite_frame_counter;

    mt_mutex (mutex) PrewriteQueue prewrite_queue;
    mt_mutex (mutex) Count prewrite_queue_size;

    mt_mutex (mutex) void recordStreamHeaders ();
    mt_mutex (mutex) void recordPrewrite ();

    mt_mutex (mutex) void recordAudioMessage (AudioMessage * mt_nonnull audio_msg);
    mt_mutex (mutex) void recordVideoMessage (VideoMessage * mt_nonnull video_msg);

    mt_mutex (mutex) void recordMessage (MediaMessage * mt_nonnull msg,
                                         bool           is_audio_msg,
                                         ConstMemory    header);

    mt_mutex (mutex) void doRecordMessage (MediaMessage * mt_nonnull msg,
                                           ConstMemory    header,
                                           Time           unixtime_timestamp_nanosec);

    mt_mutex (mutex) Result openVdatFile (ConstMemory _filename,
                                          Time        start_unixtime_nanosec);

    mt_mutex (mutex) Result openIdxFile  (ConstMemory _filename);

    mt_mutex (mutex) Result doStartRecording (Time cur_unixtime_nanosec);

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void senderStateChanged (SenderState  sender_state,
                                      void       *_recording);

      static void senderClosed (Exception *_exc,
                                void      *_recording);
    mt_iface_end

    mt_iface (VideoStream::Events)
      static VideoStream::Events const stream_events;

      static void stream_mediaDesc (MediaDesc_Audio *media_desc__audio,
                                    bool             set_audio,
                                    MediaDesc_Video *media_desc__video,
                                    bool             set_video,
                                    void            *_stream_ticket);

      static void stream_audioMessage (AudioMessage * mt_nonnull audio_msg,
                                       void *_stream_ticket);

      static void stream_videoMessage (VideoMessage * mt_nonnull video_msg,
                                       void *_stream_ticket);

      static void stream_closed (void *_stream_ticket);
    mt_iface_end

  public:
    void setVideoStream (VideoStream *stream);

    void startRecording ();
    void stopRecording  ();

    bool isRecording ();

    void init (PagePool            * mt_nonnull page_pool,
               ServerThreadContext * mt_nonnull thread_ctx,
               Vfs                 * mt_nonnull vfs,
               NamingScheme        * mt_nonnull naming_scheme,
               ConstMemory          channel_name,
               Time                 prewrite_nanosec,
               Count                prewrite_num_frames_limit,
               Time                 postwrite_nanosec,
               Count                postwrite_num_frames_limit);

     MediaRecorder (EmbedContainer *embed_container);
    ~MediaRecorder ();
};

}


#endif /* MOMENT_NVR__MEDIA_RECORDER__H__ */

