/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MP4_READER__H__
#define MOMENT__MP4_READER__H__


#include <moment/vod_reader.h>


namespace Moment {

mt_unsafe class Mp4Reader : public VodReader
{
  private:
    struct Box
    {
        Uint64 offset;
        Uint64 content_offset;
        Uint64 size;

        Size toString_ (Memory const &mem,
                        Format const & /* fmt */) const
        {
            return printToString (mem, "offset ", offset, ", content_offset ", content_offset, ", size ", size);
        }

        void setNull ()
        {
            offset = 0;
            size   = 0;
        }

        bool isNull () const { return size == 0; }

        Box ()
            : offset         (0),
              content_offset (0),
              size           (0)
        {}

        Box (Uint64 const offset,
             Uint64 const content_offset,
             Uint64 const size)
            : offset         (offset),
              content_offset (content_offset),
              size           (size)
        {}
    };

    struct Table
    {
        Box box;
        PagePool *page_pool;
        PagePool::Page *page;
        Uint64 num_entries;

        // byte offset of the current page from the beginning of the file
        Uint64 page_pos;
        // current byte offset within the page
        Uint64 pos;

        Table ()
            : page_pool   (NULL),
              page        (NULL),
              num_entries (0),
              page_pos    (0),
              pos         (0)
        {}

        ~Table ()
        {
            if (page) {
                assert (page_pool);
                page_pool->pageUnref (page);
            }
        }
    };

    struct Track
    {
        Table stts;
        Table ctts;
        Table stss;
        Table stsz;
        Table stsc;
        Table stco;

        Uint32 mdhd_timescale;
        Uint32 stsz_sample_size;

        Uint64 cur_chunk_idx;
        Uint64 cur_sample_idx;
        Uint64 cur_sample_idx_in_chunk;
        Uint64 cur_sample_offset_in_chunk;

        Uint32 cur_chunk_samples_per_chunk;
        Uint32 cur_chunk_sample_description_index;

        Uint64 cur_chunk_offset;
        Uint64 cur_stco_chunk_idx;

        Uint32 next_stsc_chunk_idx;
        bool   got_next_stsc_chunk_idx;

        Uint32 cur_stts_sample_count;
        Uint64 cur_stts_sample_delta_nanosec;
        Uint64 cur_sample_stts_idx;

        Uint32 cur_ctts_sample_count;
        Uint64 cur_ctts_sample_offset_nanosec;
        Uint64 cur_sample_ctts_idx;

        Uint64 cur_sample_dts_nanosec;

        Uint32 last_reported_sample_description_index;

        // 'track_end_reached == true' indicates that other Track fields are in
        // inconsistent state.
        bool track_end_reached;

        Track ()
            : mdhd_timescale                         (0),
              stsz_sample_size                       (0),
              cur_chunk_idx                          (0),
              cur_sample_idx                         (0),
              cur_sample_idx_in_chunk                (0),
              cur_sample_offset_in_chunk             (0),
              cur_chunk_samples_per_chunk            (0),
              cur_chunk_sample_description_index     (0),
              cur_chunk_offset                       (0),
              cur_stco_chunk_idx                     (0),
              next_stsc_chunk_idx                    (0),
              got_next_stsc_chunk_idx                (true),
              cur_stts_sample_count                  (0),
              cur_stts_sample_delta_nanosec          (0),
              cur_sample_stts_idx                    (0),
              cur_ctts_sample_count                  (0),
              cur_ctts_sample_offset_nanosec         (0),
              cur_sample_ctts_idx                    (0),
              cur_sample_dts_nanosec                 (0),
              last_reported_sample_description_index (0),
              track_end_reached                      (false)
        {}
    };

    mt_const Ref<PagePool> page_pool;

    Track audio_track;
    Track video_track;

    bool got_audio_track;
    bool got_video_track;

    struct AvcSampleDescription
    {
        Byte *avc_cdata_buf;
        Size  avc_cdata_len;

        AvcSampleDescription ()
            : avc_cdata_buf (NULL),
              avc_cdata_len (0)
        {}

        ~AvcSampleDescription ()
        {
            delete[] avc_cdata_buf;
        }
    };

    struct AacSampleDescription
    {
        Byte *aac_cdata_buf;
        Size  aac_cdata_len;

        AacSampleDescription ()
            : aac_cdata_buf (NULL),
              aac_cdata_len (0)
        {}

        ~AacSampleDescription ()
        {
            delete[] aac_cdata_buf;
        }
    };

    AvcSampleDescription *avc_sample_descs;
    AacSampleDescription *aac_sample_descs;

    Size num_avc_sample_descs;
    Size num_aac_sample_descs;

    StRef<String> filename;
    NativeFile file;

    bool header_parsed;
    bool error_state;

    mt_const Uint32 audio_rate;
    mt_const Uint32 audio_channels;

    Result readBytes (Memory mem);

    enum ReadBoxResult
    {
        ReadBoxResult_Normal,
        ReadBoxResult_Error,
        ReadBoxResult_EndOfBox
    };

    ReadBoxResult readBoxHeader (Memory       box_header_mem,
                                 Uint64       max_box_size,
                                 Uint64      * mt_nonnull ret_box_size,
                                 Uint64      * mt_nonnull ret_content_offset,
                                 ConstMemory * mt_nonnull ret_box_type);

    Result getNextBox (Box const &top_box,
                       Uint64    * mt_nonnull offset,
                       Memory     ret_box_type,
                       Box       * mt_nonnull ret_box);

    Result doGetBox (Box const   &top_box,
                     ConstMemory  desired_box_type,
                     Box         * mt_nonnull ret_box);

    template <class ...Args>
    Result getBox (Box const   &top_box,
                   ConstMemory  desired_box_type,
                   Box         * mt_nonnull ret_box,
                   Args const  &...args);

    Result prepareTable (Table     * mt_nonnull table,
                         Box const &box,
                         Size       table_entry_size,
                         Size       data_offset);

    ReadResult readTableIfNeeded (Table * mt_nonnull table,
                                  Size   table_entry_size,
                                  Size   data_offset);

    Result parseDescriptorHead (Uint64 *ret_body_offset,
                                Byte   *ret_tag,
                                Uint64 *ret_size);

    Result parseAudioEsDescriptor (Size sample_desc_idx);

    Result parseAudioSampleEntry (ConstMemory sample_entry_box_type,
                                  Uint64      sample_entry_box_size,
                                  Uint64      sample_entry_content_offset,
                                  Size        sample_desc_idx);

    Result parseVisualSampleEntry (ConstMemory sample_entry_box_type,
                                   Uint64      sample_entry_box_size,
                                   Uint64      sample_entry_content_offset,
                                   Size        sample_desc_idx);

    Result parseStsd (Track     * mt_nonnull track,
                      Box const &stsd_box,
                      bool       is_audio);

    Result parseTrack (Box const &track_box);

    Result parseHeader ();

    struct SampleInfo
    {
        Uint64 sample_size;
        Uint64 sample_offset;
        Size   sample_description_index;
        Uint64 sample_dts_nanosec;
        Uint64 sample_cts_nanosec;
    };

    void advanceFrame (Track  * mt_nonnull track,
                       Uint32  sample_size);

    ReadResult readSttsIfNeeded (Track * mt_nonnull track);
    ReadResult readCttsIfNeeded (Track * mt_nonnull track);
    ReadResult readStscIfNeeded (Track * mt_nonnull track);
    ReadResult readStcoIfNeeded (Track * mt_nonnull track);

    ReadResult seekTrack (Track * mt_nonnull track,
                          Time   seek_pos_microsec);

    ReadResult peekNextFrame (Track      * mt_nonnull track,
                              SampleInfo * mt_nonnull ret_sample_info);

    ReadResult peekNextValidFrame (Track      * mt_nonnull track,
                                   SampleInfo * mt_nonnull ret_sample_info);

    enum MediaDescResult {
        MediaDescResult_Normal,
        MediaDescResult_SameDesc,
        MediaDescResult_NoDesc
    };

    MediaDescResult getAacMediaDescForFrame (Size                  sample_description_index,
                                             Size                  last_reported_sample_description_index,
                                             Ref<MediaDesc_Audio> * mt_nonnull ret_media_desc__audio);

    MediaDescResult getAvcMediaDescForFrame (Size                  sample_description_index,
                                             Size                  last_reported_sample_description_index,
                                             Ref<MediaDesc_Video> * mt_nonnull ret_media_desc__video);

    ReadResult readNextAudioFrame (ReadMessage   * mt_nonnull read_msg,
                                   MediaMessage ** mt_nonnull ret_msg,
                                   bool           stop_time_enabled,
                                   Time           stop_time_microsec,
                                   SampleInfo    * mt_nonnull sample_info);

    ReadResult readNextVideoFrame (ReadMessage   * mt_nonnull read_msg,
                                   MediaMessage ** mt_nonnull ret_msg,
                                   bool           stop_time_enabled,
                                   Time           stop_time_microsec,
                                   SampleInfo    * mt_nonnull sample_info);

  public:
    mt_iface (VodReader)
      ReadResult readNextFrame (ReadMessage   * mt_nonnull read_msg,
                                MediaMessage ** mt_nonnull ret_msg,
                                bool           stop_time_enabled,
                                Time           stop_time_microsec);

      SeekResult seek (Time seek_pos_microsec);

      Result getSourceInfo (VodSourceInfo * mt_nonnull source_info);
    mt_iface_end

    Result open (ConstMemory filename);

    mt_const void init (PagePool * mt_nonnull page_pool);

     Mp4Reader ();
    ~Mp4Reader ();
};

}


#endif /* MOMENT__MP4_READER__H__ */

