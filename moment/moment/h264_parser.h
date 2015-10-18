/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__H264_PARSER__H__
#define MOMENT__H264_PARSER__H__


#include <libmary/libmary.h>


#define MOMENT__H264_PARSER__MAX_SPS_COUNT  32
#define MOMENT__H264_PARSER__MAX_PPS_COUNT 256


namespace Moment {

using namespace M;

mt_unsafe class H264Parser
{
  public:
    struct Callbacks
    {
        // @pts, @dts - timestamp of the access unit to which
        // the parameter sets belong.
        void (*parameterSets) (PagePool::PageListHead *page_list,
                               Size                    msg_offs,
                               Size                    msg_len,
                               Time                    pts,
                               Time                    dts,
                               void                  *cb_data);

        void (*accessUnit) (PagePool::PageListHead *page_list,
                            Size                    msg_offs,
                            Size                    msg_len,
                            bool                    is_keyframe,
                            Time                    pts,
                            Time                    dts,
                            void                   *cb_data);
    };

  private:
    mt_const Ref<PagePool> page_pool;

    mt_const Callbacks const *cb      = NULL;
    mt_const void            *cb_data = NULL;

    Buffer sps [MOMENT__H264_PARSER__MAX_SPS_COUNT];
    Buffer pps [MOMENT__H264_PARSER__MAX_PPS_COUNT];
    bool parameter_sets_changed = false;

    PagePool::PageListHead au_page_list;
    Size au_len         = 0;
    Time au_pts         = 0;
    Time au_dts         = 0;
    bool au_is_keyframe = false;
    bool got_au_begin   = false;

    enum AuDelimiterStatus {
        AuDelimiterStatus_GotAuDelimiter,
        AuDelimiterStatus_GotAuDelimiterOnly,
        AuDelimiterStatus_NoAuDelimiter
    };

    AuDelimiterStatus au_delimiter_status = AuDelimiterStatus_NoAuDelimiter;

    void reportParameterSets ();

    void doCompleteAccessUnit ();

    void maybeCompleteAccessUnit (Byte next_nal_type);

    void resetAccessUnit ();

    void parseSpsNal (PagePool::Page *page,
                      Size            msg_offs,
                      Size            msg_len);

    void parsePpsNal (PagePool::Page *page,
                      Size            msg_offs,
                      Size            msg_len);

  public:
    // AVCDecoderConfigurationRecord, ISO/IEC 14496-15:2004 p5.2.4.1.1
    bool getAvcDecoderConfigurationRecord (PagePool::PageListHead * mt_nonnull ret_page_list,
                                           Size                   * mt_nonnull ret_msg_offs,
                                           Size                   * mt_nonnull ret_msg_len);

    void parseOutOfBandParametersNal (ConstMemory nal_mem);

    void parseNal (PagePool::Page *page,
                   Size            msg_offs,
                   Size            msg_len,
                   Time            pts,
                   Time            dts,
                   bool            last_in_access_unit);

    void parseAvcNals (PagePool::Page *page,
                       Size            msg_offs,
                       Size            msg_len,
                       Time            pts,
                       Time            dts);

    void parseByteStreamNals (PagePool::Page *page,
                              Size            msg_offs,
                              Size            msg_len,
                              Time            pts,
                              Time            dts);

    void forceAccessUnitEnd ();

    mt_const void init (PagePool        * mt_nonnull page_pool,
                        Callbacks const * mt_nonnull cb,
                        void            *cb_data);
};

}


#endif /* MOMENT__H264_PARSER__H__ */

