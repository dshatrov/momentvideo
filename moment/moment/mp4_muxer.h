/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MP4_MUXER__H__
#define MOMENT__MP4_MUXER__H__


#include <moment/media_desc.h>


namespace Moment {

mt_unsafe class Mp4Muxer
{
  public:
    enum FrameType {
        FrameType_Audio,
        FrameType_Video
    };

  private:
    struct TrackInfo
    {
        PagePool       *hdr_page_pool;
        PagePool::Page *hdr_msg;
        Size            hdr_offs;
        Size            hdr_size;

        Size num_frames;
        Size total_frame_size;

        PagePool::PageListInfo stsz_pages;
        Size stsz_pos;

        PagePool::PageListInfo stss_pages;
        Size stss_pos;
        Count num_stss_entries;

        PagePool::PageListInfo stts_pages;
        Size stts_pos;
        Time prv_stts_value;

        PagePool::PageListInfo ctts_pages;
        Size ctts_pos;

        PagePool::PageListInfo stco_pages;
        Size stco_pos;

        Time prv_pts;
        Time min_pts;

        void clear (PagePool * mt_nonnull page_pool);

        TrackInfo ()
            : hdr_page_pool    (NULL),
              hdr_msg          (NULL),
              hdr_offs         (0),
              hdr_size         (0),
              num_frames       (0),
              total_frame_size (0),
              stsz_pos         (0),
              stss_pos         (0),
              num_stss_entries (0),
              stts_pos         (0),
              prv_stts_value   (0),
              ctts_pos         (0),
              stco_pos         (0),
              prv_pts          (0),
              min_pts          (0)
        {}
    };

    mt_const Ref<PagePool> page_pool;
    mt_const bool derive_dts_from_pts;

    Ref<MediaDesc_Audio> media_desc__audio;
    Ref<MediaDesc_Video> media_desc__video;

    Uint32 avc_hdr_width;
    Uint32 avc_hdr_height;

    TrackInfo audio_track;
    TrackInfo video_track;

    Uint64 mdat_pos;

    void patchTrackStco (TrackInfo * mt_nonnull track,
                         Uint32     offset);

    PagePool::PageListInfo writeMoovAtom (Time duration_millisec,
                                          bool moov_only,
                                          bool moov_tail);

    void processFrame (TrackInfo * mt_nonnull track,
                       Time       pts_nanosec,
                       Time       dts_nanosec,
                       Size       frame_size,
                       bool       is_sync_sample);

    void finalizeTrack (TrackInfo * mt_nonnull track,
                        Time       duration_millisec);

  public:
    void pass1_mediaDesc (MediaDesc_Audio *audio_desc,
                          bool             set_audio,
                          MediaDesc_Video *video_desc,
                          bool             set_video);

    void pass1_aacSequenceHeader (PagePool       * mt_nonnull msg_page_pool,
                                  PagePool::Page *msg,
                                  Size            msg_offs,
                                  Size            frame_size);

    void pass1_avcSequenceHeader (PagePool       * mt_nonnull msg_page_pool,
                                  PagePool::Page *msg,
                                  Size            msg_offs,
                                  Size            frame_size);

    // TODO AVC file format has restrictions on which NALs should appear in mp4.
    void pass1_frame (FrameType frame_type,
                      Time      pts_nanosec,
                      Time      dts_nanosec,
                      Size      frame_size,
                      bool      is_sync_sample);

    PagePool::PageListInfo pass1_complete (Time duration_millisec,
                                           bool moov_only = false,
                                           bool moov_tail = false);

    Size getTotalDataSize () const
        { return audio_track.total_frame_size + video_track.total_frame_size; }

    void clear ();

    mt_const void init (PagePool * mt_nonnull page_pool,
                        bool      derive_dts_from_pts);

    Mp4Muxer ()
        : avc_hdr_width  (640),
          avc_hdr_height (480),
          mdat_pos       (0)
    {}

    ~Mp4Muxer ();
};

}


#endif /* MOMENT__MP4_MUXER__H__ */

