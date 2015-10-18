#ifndef MOMENT__TS_DEMUXER__H__
#define MOMENT__TS_DEMUXER__H__


#include <moment/media_message.h>
#include <moment/h264_parser.h>


namespace Moment {

mt_unsafe class TsDemuxer
{
  public:
    struct TsHandler {
        Result (*mediaMessage) (MediaMessage *msg,
                                void         *cb_data);
    };

  private:
    Ref<PagePool> page_pool;

    TsHandler const *ts_handler;
    void            *ts_handler_data;

    struct AccumulatedPacket
    {
        PagePool::PageListHead page_list;
        Size packet_len;

        AccumulatedPacket ()
            : packet_len (0)
        {}
    };

    AccumulatedPacket audio_pes_packet;
    AccumulatedPacket video_pes_packet;

    H264Parser h264_parser;

    void fillAndReportMediaMessage (MediaMessage * mt_nonnull msg);

    mt_iface (H264Parser::Callbacks)
      static H264Parser::Callbacks const h264_callbacks;

      static void h264_parameterSets (PagePool::PageListHead *page_list,
                                      Size                    msg_offs,
                                      Size                    msg_len,
                                      Time                    pts,
                                      Time                    dts,
                                      void                   *_self);

      static void h264_accessUnit (PagePool::PageListHead *page_list,
                                   Size                    msg_offs,
                                   Size                    msg_len,
                                   bool                    is_keyframe,
                                   Time                    pts,
                                   Time                    dts,
                                   void                   *_self);
    mt_iface_end

    void processPesPacket (AccumulatedPacket * mt_nonnull packet,
                           bool               is_audio);

    Result processTsPacket (ConstMemory mem);

  public:
    Result processData (ConstMemory  mem,
                        Size        * mt_nonnull ret_accepted);

    void setHandler (TsHandler const * const ts_handler,
                     void            * const ts_handler_data)
    {
        this->ts_handler      = ts_handler;
        this->ts_handler_data = ts_handler_data;
    }

    void init (PagePool * mt_nonnull page_pool);

    TsDemuxer ()
        : ts_handler      (NULL),
          ts_handler_data (NULL)
    {}

    ~TsDemuxer ();
};

}


#endif /* MOMENT__TS_DEMUXER__H__ */

