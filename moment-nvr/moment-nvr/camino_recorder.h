/*  Copyright (C) 2014-2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__CAMINO_RECORDER__H__
#define MOMENT__CAMINO_RECORDER__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace Moment;

class CaminoRecorder : public Object
{
  private:
    StateMutex mutex;

    struct Recording : public Object
    {
        WeakRef<CaminoRecorder> weak_camino_recorder;

        mt_mutex (mutex) StRef<Vfs::VfsFile> vdat_file;
        Embed<FileConnection>                vdat_conn;
        ConnectionSenderImpl                 vdat_sender_impl;

        mt_mutex (mutex) StRef<Vfs::VfsFile> idx_file;
        Embed<FileConnection>                idx_conn;
        ConnectionSenderImpl                 idx_sender_impl;

        mt_mutex (mutex) Size cur_vdat_offset;

        Recording (EmbedContainer * const embed_container);
    };

    class WriteQueue_name;

    struct WriteQueueEntry : public IntrusiveListElement<WriteQueue_name>
    {
        enum Type
        {
            t_Audio,
            t_Video
        };

        Type const type;

        WriteQueueEntry (Type const type)
            : type (type)
        {}

        virtual ~WriteQueueEntry () {}
    };

    struct WriteQueueEntry_Audio : public WriteQueueEntry
    {
        AudioMessage audio_msg;

        WriteQueueEntry_Audio ()
            : WriteQueueEntry (t_Audio)
        {}
    };

    struct WriteQueueEntry_Video : public WriteQueueEntry
    {
        VideoMessage video_msg;

        WriteQueueEntry_Video ()
            : WriteQueueEntry (t_Video)
        {}
    };

    typedef IntrusiveList< WriteQueueEntry,
                           WriteQueue_name,
                           DeleteAction<WriteQueueEntry> > WriteQueue;

    class PrewriteQueue_name;

    struct PrewriteQueueEntry : public IntrusiveListElement<PrewriteQueue_name>
    {
        enum Type
        {
            t_Audio,
            t_Video,
            t_MediaDesc
        };

        Type const type;

        Time getDtsNanosec () const;

        PrewriteQueueEntry (Type const type)
            : type (type)
        {}

        virtual ~PrewriteQueueEntry () {}
    };

    struct PrewriteQueueEntry_Audio : public PrewriteQueueEntry
    {
        AudioMessage audio_msg;

        PrewriteQueueEntry_Audio (Type const type)
            : PrewriteQueueEntry (type)
        {}

        ~PrewriteQueueEntry_Audio ()
        {
            audio_msg.release ();
        }
    };

    struct PrewriteQueueEntry_Video : public PrewriteQueueEntry
    {
        VideoMessage video_msg;

        PrewriteQueueEntry_Video (Type const type)
            : PrewriteQueueEntry (type)
        {}

        ~PrewriteQueueEntry_Video ()
        {
            video_msg.release ();
        }
    };

#error This is likely unnecessary
    struct PrewriteQueueEntry_MediaDesc : public PrewriteQueueEntry
    {
        MediaDescMessage media_desc_msg;

        PrewriteQueueEntry_MediaDesc (Type const type)
            : PrewriteQueueEntry (type)
        {}

        ~PrewriteQueueEntry_MediaDesc ()
        {
            media_desc_msg.release ();
        }
    };

    typedef IntrusiveList< PrewriteQueueEntry,
                           PrewriteQueue_name,
                           DeleteAction<PrewriteQueueEntry> > PrewriteQueue;

    mt_const Ref<PagePool>            page_pool;
    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const StRef<Vfs>               vfs;

    mt_const StRef<String> channel_name;

    mt_const Size  write_queue_max_total_pagesize;
    mt_const Count write_queue_max_num_entries;

    mt_const Time  prewrite_microsec;
    mt_const Count prewrite_num_frames_limit;
    mt_const Time  postwrite_microsec;
    mt_const Count postwrite_num_frames_limit;

    struct StreamTicket : public Referenced
    {
        CaminoRecorder * const camino_recorder;

        StreamTicket (CaminoRecorder * const mt_nonnull camino_recorder)
            : camino_recorder (camino_recorder)
        {}
    };

    mt_mutex (mutex) Ref<StreamTicket> cur_stream_ticket;
    mt_mutex (mutex) Ref<MediaStream>  cur_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey stream_sbn;

    mt_mutex (mutex) bool got_unixtime_offset;
    mt_mutex (mutex) Time unixtime_offset_nanosec;
    mt_mutex (mutex) Time prv_unixtime_dts_nanosec;

    mt_mutex (mutex) bool got_aac_cdata;
    mt_mutex (mutex) bool got_pending_aac_cdata;
    mt_mutex (mutex) Buffer pending_aac_cdata_buffer;

    mt_mutex (mutex) bool got_avc_cdata;
    mt_mutex (mutex) bool got_pending_avc_cdata;
    mt_mutex (mutex) Buffer pending_avc_cdata_buffer;

    mt_mutex (mutex) Ref<Recording> recording;
    mt_mutex (mutex) Time next_file_unixtime_nanosec;

    mt_mutex (mutex) WriteQueue write_queue;
    mt_mutex (mutex) Size       write_queue_total_pagesize;
    mt_mutex (mutex) Count      write_queue_num_entries;
    mt_mutex (mutex) bool       write_task_in_progress;

    mt_mutex (mutex) bool  postwrite_active;
    mt_mutex (mutex) bool  got_postwrite_start_dts;
    mt_mutex (mutex) Time  postwite_start_dts_nanosec;
    mt_mutex (mutex) Count postwrite_frame_counter;

    mt_mutex (mutex) PrewriteQueue prewrite_queue;
    mt_mutex (mutex) Count         prewrite_queue_size;

    DeferredProcessor::Task write_task;
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) void recordStreamHeaders ();
    mt_mutex (mutex) void recordPrewrite ();

    mt_mutex (mutex) void recordAudioMessage (AudioMessage * mt_nonnull audio_msg);
    mt_mutex (mutex) void recordVideoMessage (VideoMessage * mt_nonnull video_msg);

    mt_mutex (mutex) void recordMessage (MediaMessage * mt_nonnull msg,
                                         ConstMemory   header);

    mt_mutex (mutex) Result openVdatFile (ConstMemory _filename,
                                          Time        start_unixtime_nanosec);

    mt_mutex (mutex) Result openIdxFile (ConstMemory _filename);

    mt_mutex (mutex) Result doStartRecording (Time cur_unixtime_nanosec);

    mt_sync_domain (writeTask) Result writeMessage (WriteQueueEntry * mt_nonnull entry);

    mt_unlocks (mutex) void clearWriteQueueAndUnlock ();

    mt_sync_domain (writeTask) static bool writeTask (void *_self);

    mt_iface (MediaStream::Events)
      static MediaStream::Events const stream_events;

      static void stream_audioMessage (AudioMessage * mt_nonnull audio_msg,
                                       void         *_stream_ticket);

      static void stream_videoMessage (VideoMessage * mt_nonnull video_msg,
                                       void         *_stream_ticket);

      static void stream_closed (void *_stream_ticket);
    mt_iface_end

  public:
    void setMediaStream (MediaStream *stream);

    void startRecording ();
    void stopRecording  ();

    bool isRecording ();

    void init (PagePool            * mt_nonnull page_pool,
               ServerThreadContext * mt_nonnull thread_ctx,
               Vfs                 * mt_nonnull vfs,
               ConstMemory          channel_name,
               Time                 prewrite_microsec,
               Count                prewrite_num_frames_limit,
               Time                 postwrite_microsec,
               Count                postwrite_num_frames_limit);

     CaminoRecorder (EmbedContainer *embed_container);
    ~CaminoRecorder ();
};

}


#endif /* MOMENT__CAMINO_RECORDER__H__ */

