#include <moment/libmoment.h>


using namespace Moment;

namespace {
class HlsTool : public Object
{
  private:
    mt_const Embed<ServerApp> server_app;
    mt_const Embed<RealtimeHlsClient> hls_client;

    H264Parser h264_parser;

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

    mt_iface (VodSource::Frontend)
      static VodSource::Frontend const vod_source_frontend;

      static void vodSource_mediaMessage (MediaMessage * mt_nonnull msg,
                                          void         *_self);

      static void vodSource_endOfSource (VodSource::EndReason  end_reason,
                                         void                 *_self);
    mt_iface_end

  public:
    mt_const Result init (PagePool * mt_nonnull page_pool);

    Result run ();

    HlsTool (EmbedContainer * const embed_container)
        : Object (embed_container),
          server_app (/*embed_container=*/ this),
          hls_client (/*embed_container=*/ this)
    {}
};
}

H264Parser::Callbacks const HlsTool::h264_callbacks = {
    h264_parameterSets,
    h264_accessUnit
};

void
HlsTool::h264_parameterSets (PagePool::PageListHead * const page_list,
                             Size                     const msg_offs,
                             Size                     const msg_len,
                             Time                     const pts,
                             Time                     const dts,
                             void                   * const _self)
{
    HlsTool * const self = static_cast <HlsTool*> (_self);

    logD_ (_self_func,
           "msg_offs ", msg_offs, ", "
           "msg_len ", msg_len, ", "
           "pts ", pts, ", "
           "dts ", dts);

    PagePool::dumpPages (logs, page_list, msg_offs);
}

void
HlsTool::h264_accessUnit (PagePool::PageListHead * const page_list,
                          Size                     const msg_offs,
                          Size                     const msg_len,
                          bool                     const is_keyframe,
                          Time                     const pts,
                          Time                     const dts,
                          void                   * const _self)
{
    HlsTool * const self = static_cast <HlsTool*> (_self);

    logD_ (_self_func,
           "msg_offs ", msg_offs, ", "
           "msg_len ", msg_len, ", "
           "is_keyframe ", is_keyframe, ", "
           "pts ", pts, ", "
           "dts ", dts);

    dumpH264AvcNalUnits (page_list->first, msg_offs, msg_len);
}

VodSource::Frontend const HlsTool::vod_source_frontend = {
    vodSource_mediaMessage,
    vodSource_endOfSource
};

void
HlsTool::vodSource_mediaMessage (MediaMessage * const mt_nonnull msg,
                                 void         * const _self)
{
    HlsTool * const self = static_cast <HlsTool*> (_self);

    logD_ (_self_func_);

    self->h264_parser.parseAvcNals (msg->page_list.first,
                                    msg->msg_offset,
                                    msg->msg_len,
                                    msg->pts_nanosec,
                                    msg->dts_nanosec);
    self->h264_parser.forceAccessUnitEnd ();
}

void
HlsTool::vodSource_endOfSource (VodSource::EndReason   const end_reason,
                                void                 * const _self)
{
    HlsTool * const self = static_cast <HlsTool*> (_self);

    logD_ (_self_func, "end_reason ", end_reason);

    logD_ (_self_func, "calling server_app.stop()");
    self->server_app->stop ();
}

mt_const Result
HlsTool::init (PagePool * const mt_nonnull page_pool)
{
    logD_ (_this_func, "h264_parser " _ptr (&h264_parser));

    h264_parser.init (page_pool, &h264_callbacks, this);

    if (!server_app->init (/*num_threads=*/ 0)) {
        logE_ (_this_func, "server_app.init() failed: ", exc->toString());
        return Result::Failure;
    }

    if (!hls_client->init (server_app->getMainThreadContext(),
                           page_pool,
                           "http://127.0.0.1:8080",
                           CbDesc<VodSource::Frontend> (&vod_source_frontend, this, this)))
    {
        logE_ (_this_func, "hls_client.init() failed");
        return Result::Failure;
    }

    return Result::Success;
}

Result
HlsTool::run ()
{
    hls_client->start (/*stop_time_enabled=*/  false,
                       /*stop_time_microsec=*/ 0);

    if (!server_app->run ()) {
        logE_ (_this_func, "server_app.run() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

int main (void)
{
    libMaryInit ();

    Ref<PagePool> const page_pool = grabNewObject <PagePool> ();
    page_pool->init (/*page_size=*/                4096,
                     /*min_pages=*/                4096,
                     /*delete_immediately=*/       true,
                     /*timers=*/                   NULL,
                     /*delete_interval_microsec=*/    0,
                     /*delete_bulk_size=*/            0);

    Ref<HlsTool> const hls_tool = grabNewObject <HlsTool> ();
    if (!hls_tool->init (page_pool)) {
        logE_ (_func, "hls_tool.init() failed");
        return EXIT_FAILURE;
    }

    if (!hls_tool->run ()) {
        logE_ (_func, "hls_tool.run() failed: ", exc->toString());
        return EXIT_FAILURE;
    }
}

