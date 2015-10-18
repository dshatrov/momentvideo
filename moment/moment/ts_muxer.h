/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__TS_MUXER__H__
#define MOMENT__TS_MUXER__H__


#include <moment/media_message.h>


namespace Moment {

mt_unsafe class TsMuxer
{
  private:
    class TsFiller;

    mt_const PagePool *page_pool;

    mt_const bool got_audio;
    mt_const bool got_video;

    Byte pat_continuity_counter;
    Byte pmt_continuity_counter;
    Byte audio_continuity_counter;
    Byte video_continuity_counter;

    PagePool               *aac_cdata_page_pool;
    PagePool::PageListHead  aac_cdata_page_list;
    Size                    aac_cdata_offset;
    Size                    aac_cdata_len;

    PagePool               *avc_cdata_page_pool;
    PagePool::PageListHead  avc_cdata_page_list;
    Size                    avc_cdata_len;

    Byte nal_length_size;

    PagePool::PageListHead page_list;
    Size total_data_len;

    Byte* allocateTransportPacket ();

    static Byte* fillTsHeader (Byte   * mt_nonnull buf,
                               Uint16  payload_unit_start_indicator,
                               Uint16  pid,
                               Uint8  * mt_nonnull continuity_counter,
                               Uint8   adaptation_field_control,
                               Time    pcr,
                               Size    payload_bytes_available,
                               Uint8   adaptation_field_flags,
                               Size   * mt_nonnull ret_payload_len);

    void fillPesHeader (Byte   * mt_nonnull buf,
                        Byte    stream_id,
                        Uint64  pts,
                        Uint64  dts,
                        Size    pes_data_len,
                        Size   * mt_nonnull ret_len);

    void fillAdtsAacHeader (AudioMessage * mt_nonnull audio_msg,
                            Byte         * mt_nonnull adts_header);

    Result muxAvcCodecData (Array                   * mt_nonnull arr,
                            Size                     arr_len,
                            PagePool               ** mt_nonnull ret_page_pool,
                            PagePool::PageListHead  * mt_nonnull ret_page_list,
                            Size                    * mt_nonnull ret_msg_len,
                            Byte                    * mt_nonnull ret_nal_length_size);

    void muxAacMediaDesc (MediaDesc_AAC * mt_nonnull media_desc__aac);
    void muxAacMessage   (AudioMessage  * mt_nonnull audio_msg);

    void muxAvcMediaDesc (MediaDesc_AVC * mt_nonnull media_desc__avc);
    void muxAvcMessage   (VideoMessage  * mt_nonnull video_msg);

    void writePAT ();
    void writePMT ();

  public:
    bool getGotAudio () { return got_audio; }
    bool getGotVideo () { return got_video; }

    void writeProgramTables ();

    void setNalLengthSize (Byte const nal_length_size) { this->nal_length_size = nal_length_size; }

    void resetAvcCodecData ()
    {
        if (avc_cdata_page_pool) {
            avc_cdata_page_pool->msgUnref (avc_cdata_page_list.first);
            avc_cdata_page_list.reset ();
            avc_cdata_page_pool = NULL;
            avc_cdata_len = 0;
        }
    }

    // It's ok to mux a single NAL with several calls to muxSpsOrPpsAnnexBNal().
    //
    void muxSpsOrPpsAnnexBNal (Array * const mt_nonnull arr,
                               Size    const len)
    {
        page_pool->getFillPagesFromArray (&avc_cdata_page_list, arr, /*offset=*/ 0, len);
        avc_cdata_len += len;

        if (!avc_cdata_page_pool)
            avc_cdata_page_pool = page_pool;
    }

    void muxAvcAccessUnitData (Array * mt_nonnull arr,
                               Size   msg_len,
                               Uint64 pts_nanosec,
                               Uint64 dts_nanosec);

    void muxMediaMessage (MediaMessage * mt_nonnull msg);

    void stuffContinuityCounters ();

    PagePool::PageListHead flush (Size * mt_nonnull ret_len);

    mt_const void init (PagePool * mt_nonnull page_pool,
                        bool      got_audio,
                        bool      got_video);

     TsMuxer ();
    ~TsMuxer ();
};

}


#endif /* MOMENT__TS_MUXER__H__ */

