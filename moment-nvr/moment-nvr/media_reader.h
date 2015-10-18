/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__MEDIA_READER__H__
#define MOMENT_NVR__MEDIA_READER__H__


#include <moment-nvr/types.h>
#include <moment-nvr/nvr_file_iterator.h>


namespace MomentNvr {

mt_unsafe class MediaReader : public Object
{
  public:
    enum ReadFrameResult
    {
        ReadFrameResult_Success,
        ReadFrameResult_Finish,
        ReadFrameResult_BurstLimit,
        ReadFrameResult_NoData,
        ReadFrameResult_Failure
    };

    struct ReadFrameBackend
    {
        ReadFrameResult (*audioFrame) (AudioMessage * mt_nonnull msg,
                                       void         *cb_data);

        ReadFrameResult (*videoFrame) (VideoMessage * mt_nonnull msg,
                                       void         *cb_data);
    };

  private:
    enum SessionState {
        SessionState_FileHeader,
        SessionState_SequenceHeaders,
        SessionState_Frame
    };

    mt_const Ref<PagePool> page_pool;
    mt_const StRef<Vfs> vfs;

    mt_const Time start_unixtime_sec;
    // 0 means no limit
    mt_const Size burst_size_limit;

    mt_const Time   timelapse_frame_interval_millisec;
    mt_const Uint32 timelapse_frames_per_second;

    SessionState session_state;
    NvrFileIterator file_iter;

    StRef<Vfs::VfsFile> vdat_file;
    StRef<String> cur_filename;
    Uint64 vdat_data_start;

    bool first_file;

    bool sequence_headers_sent;
    bool first_frame;

    bool got_aac_seq_hdr;
    bool aac_seq_hdr_sent;
    PagePool::PageListHead aac_seq_hdr;
    Size aac_seq_hdr_len;

    bool got_avc_seq_hdr;
    bool avc_seq_hdr_sent;
    PagePool::PageListHead avc_seq_hdr;
    Size avc_seq_hdr_len;

    void releaseSequenceHeaders_locked ();

    bool tryOpenNextFile ();
    Result readFileHeader ();
    Result readIndexAndSeek (bool * mt_nonnull ret_seeked);

    ReadFrameResult readFrame (ReadFrameBackend const *read_frame_cb,
                               void                   *read_frame_cb_data);

  public:
    void setTimelapse (Time   const timelapse_frame_interval_millisec,
                       Uint32 const timelapse_frames_per_second)
    {
        this->timelapse_frame_interval_millisec = timelapse_frame_interval_millisec;
        this->timelapse_frames_per_second = timelapse_frames_per_second;
    }

    ReadFrameResult readMoreData (ReadFrameBackend const *read_frame_cb,
                                  void                   *read_frame_cb_data);

    void reset ()
    {
        session_state = SessionState_FileHeader;
        releaseSequenceHeaders_locked ();
        first_file = true;
        sequence_headers_sent = false;
        first_frame = true;
        file_iter.reset (start_unixtime_sec);
        vdat_file = NULL;
    }

    mt_const void init (PagePool    * mt_nonnull page_pool,
                        Vfs         * mt_nonnull vfs,
                        ConstMemory  stream_name,
                        Time         start_unixtime_sec,
                        Size         burst_size_limit);

    MediaReader (EmbedContainer * const embed_container)
        : Object                (embed_container),
          start_unixtime_sec    (0),
          burst_size_limit      (0),
          timelapse_frame_interval_millisec (0),
          timelapse_frames_per_second (0),
          session_state         (SessionState_FileHeader),
          vdat_data_start       (0),
          first_file            (true),
          sequence_headers_sent (false),
          first_frame           (true),
          got_aac_seq_hdr       (false),
          aac_seq_hdr_sent      (false),
          aac_seq_hdr_len       (0),
          got_avc_seq_hdr       (false),
          avc_seq_hdr_sent      (false),
          avc_seq_hdr_len       (0)
    {}

    ~MediaReader ();
};

}


#endif /* MOMENT_NVR__MEDIA_READER__H__ */

