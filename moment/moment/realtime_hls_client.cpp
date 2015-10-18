#include <moment/realtime_hls_client.h>


namespace Moment {

static LogGroup libMary_logGroup_hlsrt ("realtime_hls_client", LogLevel::I);

TsDemuxer::TsHandler const RealtimeHlsClient::ts_handler = {
    tsMediaMessage
};

Result
RealtimeHlsClient::tsMediaMessage (MediaMessage * const msg,
                                   void         * const _self)
{
    RealtimeHlsClient * const self = static_cast <RealtimeHlsClient*> (_self);

    logD (hlsrt, _self_func, *msg);

    if (self->frontend)
        self->frontend.call (self->frontend->mediaMessage, /*(*/ msg /*)*/);

    return Result::Success;
}

HttpClient::HttpResponseHandler const RealtimeHlsClient::hls_segment_response_handler = {
    hlsSegmentHttpResponse,
    hlsSegmentHttpResponseBody
};

Result
RealtimeHlsClient::hlsSegmentHttpResponse (HttpRequest  * const resp,
                                           Memory        msg_body,
                                           void        ** const mt_nonnull /* ret_msg_data */,
                                           void         * const _self)
{
    RealtimeHlsClient * const self = static_cast <RealtimeHlsClient*> (_self);

    logD (hlsrt, _self_func_);

    // TODO handle errors

    return Result::Success;
}

Result
RealtimeHlsClient::hlsSegmentHttpResponseBody (HttpRequest  * const resp,
                                               Memory         const mem,
                                               bool           const end_of_response,
                                               Size         * const mt_nonnull ret_accepted,
                                               void         * const msg_data,
                                               void         * const _self)
{
  // TODO Make sure that all responses arrive in the same thread, so that mt_unsafe ts_demuxer is fine.

    RealtimeHlsClient * const self = static_cast <RealtimeHlsClient*> (_self);

//    logD (hlsrt, _self_func, "mem.len ", mem.len(), ", end_of_response ", end_of_response);

    if (!self->ts_demuxer.processData (mem, ret_accepted)) {
        logD (hlsrt, _self_func, "ts_demuxer.processData() failed");
        *ret_accepted = mem.len();
        return Result::Failure; // discard response
    }

    return Result::Success;
}

HttpClient::HttpResponseHandler const RealtimeHlsClient::hls_playlist_response_handler = {
    hlsPlaylistHttpResponse,
    /*httpResponseBody=*/ NULL
};

static void getNextHlsPlaylistLine (Byte const * const buf,
                                    Size         const len,
                                    Size       * const mt_nonnull ret_line_len,
                                    Size       * const mt_nonnull ret_next_line_offs)
{
    Size line_len = 0;
    Size next_line_offs = 0;
    for (;;) {
        if (line_len >= len) {
            next_line_offs = len;
            break;
        }

        if (len - line_len >= 1
            && buf [line_len] == '\n')
        {
            next_line_offs = line_len + 1;
            break;
        }

        if (len - line_len >= 2
            && buf [line_len + 0] == '\r'
            && buf [line_len + 1] == '\n')
        {
            next_line_offs = line_len + 2;
            break;
        }

        ++line_len;
    }

    *ret_line_len       = line_len;
    *ret_next_line_offs = next_line_offs;
}

Result
RealtimeHlsClient::hlsPlaylistHttpResponse (HttpRequest  * const resp,
                                            Memory        msg_body,
                                            void        ** const mt_nonnull /* ret_msg_data */,
                                            void         * const _self)
{
    RealtimeHlsClient * const self = static_cast <RealtimeHlsClient*> (_self);

    logD (hlsrt, _self_func,
          "resp: 0x", fmt_hex, (UintPtr) resp, ", "
          "code ", fmt_def, (resp ? resp->getResponseCode() : 0), ", "
          "msg_body.len ", msg_body.len());
//    if (resp)
//        hexdump (logs, msg_body);

    if (!(resp && resp->getResponseCode() == 200)) {
        if (self->frontend)
            self->frontend.call (self->frontend->endOfSource, VodSource::EndReason::Error);

        return Result::Success;
    }


    Byte const *buf = msg_body.buf();
    Size        len = msg_body.len();
    while (len > 0) {
        Size line_len = 0;
        Size next_line_offs = 0;

        getNextHlsPlaylistLine (buf, len, &line_len, &next_line_offs);

        if (line_len >= 4 && !memcmp (buf, "#EXT", 4)) {
            // tag

            logD (hlsrt, _self_func, "tag: \"", ConstMemory (buf, line_len), "\"");

            ConstMemory const tag_body = ConstMemory (buf + 1, line_len - 1);

            if (stringHasPrefix (tag_body, "EXT-X-MEDIA-SEQUENCE")) {
                logD (hlsrt, _self_func, "media sequence");
                // TODO
            } else
            if (stringHasPrefix (tag_body, "EXTINF")) {
                logD (hlsrt, _self_func, "EXTINF tag, the next line is an URI");

                buf += next_line_offs;
                len -= next_line_offs;
                getNextHlsPlaylistLine (buf, len, &line_len, &next_line_offs);

                if (line_len >= 1 && buf [0] != '#') {
                    ConstMemory segment_uri = ConstMemory (buf, line_len);
                    StRef<String> segment_uri_str;
                    if (segment_uri.len() && segment_uri.buf() [0] != '/') {
                        // constructing absolute uri
                        segment_uri_str = makeString ("hls/vod/bunny.mp4/", segment_uri);
//                        segment_uri_str = makeString ("hls/", segment_uri);
                        segment_uri = segment_uri_str->mem();
                    }

                    logD (hlsrt, _self_func, "SEGMENT URI: \"", segment_uri, "\"");

                    if (!self->http_client->httpGet (segment_uri,
                                                     CbDesc<HttpClient::HttpResponseHandler> (&hls_segment_response_handler, self, self),
                                                     /*preassembly=*/ false))
                    {
                        logD (hlsrt, _self_func, "http_client.httpGet() failed");
                        self->mutex.unlock ();

                        if (self->frontend)
                            self->frontend.call (self->frontend->endOfSource, VodSource::EndReason::Error);

                        return Result::Success;
                    }

                    // TEST mode: downloading only the 1st segment.
                    return Result::Success;
                }
            }
        } else {
            // comment or uri

            if (line_len >= 1 && buf [0] == '#')
                logD (hlsrt, _self_func, "comment: \"", ConstMemory (buf, line_len), "\"");
            else
                logD (hlsrt, _self_func, "uri: \"", ConstMemory (buf, line_len), "\"");
        }

        assert (next_line_offs <= len);
        buf += next_line_offs;
        len -= next_line_offs;
    }

    return Result::Success;
}

mt_unlocks (mutex) void
RealtimeHlsClient::doStart ()
{
    logD (hlsrt, _this_func, "calling http_client.httpGet()");

    if (!http_client->httpGet ("hls/vod/bunny.mp4/p.m3u8",
//    if (!http_client->httpGet ("hls/test.m3u8",
                               CbDesc<HttpClient::HttpResponseHandler> (&hls_playlist_response_handler, this, this),
                               /*preassembly=*/ true))
    {
        logD (hlsrt, _this_func, "http_client.httpGet() failed");
        mutex.unlock ();

        if (frontend)
            frontend.call (frontend->endOfSource, VodSource::EndReason::Error);

        return;
    }

    mutex.unlock ();
}

void
RealtimeHlsClient::hostnameLookupCallback (IpAddress_NoPort * const addr,
                                           void             * const _self)
{
    RealtimeHlsClient * const self = static_cast <RealtimeHlsClient*> (_self);

    self->mutex.lock ();
    if (!addr) {
        logE (hlsrt, _self_func, "hostnameLookup (\"", self->host, "\" failed");
        self->mutex.unlock ();

        if (self->frontend)
            self->frontend.call (self->frontend->endOfSource, VodSource::EndReason::Error);

        return;
    }

    mt_unlocks (mutex) self->completeHostnameLookup (addr);
}

mt_unlocks (mutex) void
RealtimeHlsClient::completeHostnameLookup (IpAddress_NoPort * const mt_nonnull addr)
{
    logD (hlsrt, _this_func, "addr 0x", fmt_hex, (UintPtr) addr, " (0x", (addr ? addr->ip_addr : 0), ")");

    IpAddress addr_with_port;
    setIpAddress (addr->ip_addr, port, &addr_with_port);

    http_client->init (thread_ctx,
                       page_pool,
                       addr_with_port,
                       host,
                       /*keepalive=*/ true,
                       /*preassembly_limit=*/ 16777216);

    assert (!hostname_resolved);
    hostname_resolved = true;

    if (start_pending)
        mt_unlocks (mutex) doStart ();
    else
        mutex.unlock ();
}

void
RealtimeHlsClient::start (bool const stop_time_enabled,
                          Time const stop_time_microsec)
{
    mutex.lock ();
    stopped = false;

    if (!hostname_resolved) {
        start_pending = true;
        mutex.unlock ();
        return;
    }

    mt_unlocks (mutex) doStart ();
}

void
RealtimeHlsClient::stop ()
{
    mutex.lock ();
    stopped = true;
    start_pending = false;

    // TODO
    mutex.unlock ();
}

void
RealtimeHlsClient::block ()
{
}

void
RealtimeHlsClient::resume ()
{
}

void
RealtimeHlsClient::seek (Time const seek_pos_microsec)
{
    logD (hlsrt, _this_func, "unsupported");
}

bool
RealtimeHlsClient::querySourceInfo (CbDesc<VodSource::QuerySourceInfoCallback> const &cb,
                                    VodSourceInfo * mt_nonnull const ret_source_info)
{
    return false;
}

mt_const Result
RealtimeHlsClient::init (ServerThreadContext * const mt_nonnull thread_ctx,
                         PagePool            * const mt_nonnull page_pool,
                         ConstMemory           const uri_mem,
                         CbDesc<VodSource::Frontend> const &frontend)
{
#warning setFrontend() after init() means that init() can't initiate async ops (hostname resolution)
#warning ^ move frontend setting into init, where it belongs

    uri = uri_mem;
    this->thread_ctx = thread_ctx;
    this->page_pool  = page_pool;
    this->frontend   = frontend;

    ts_demuxer.init (page_pool);
    ts_demuxer.setHandler (&ts_handler, this);

    ConstMemory tmp_host;
    if (!parseUri (uri_mem,
                   /*default_port=*/ port,
                   /*ret_proto=*/    NULL,
                   /*ret_login=*/    NULL,
                   /*ret_password=*/ NULL,
                   &tmp_host,
                   &port,
                   /*ret_path=*/     NULL))
    {
        logE (hlsrt, _this_func, "parseUri (\"", uri_mem, "\") failed");
        return Result::Failure;
    }
    host = tmp_host;

    logD (hlsrt, _this_func, "host \"", host, "\", port ", port);

    IpAddress_NoPort addr ((IpAddress()));
    Result res = Result::Failure;
    if (hostnameLookup_async (host,
                              CbDesc<HostnameLookupCallback> (hostnameLookupCallback, this, this),
                              &addr,
                              &res,
                              thread_ctx->getTimers(),
                              /* TODO Global default DNS timeout + local override */
                              /*timeout_microsec=*/ 60 * 1000000))
    {
        if (!res) {
            logE (hlsrt, _this_func, "hostnameLookup_async (\"", host, "\") failed");
            return Result::Failure;
        }

        mutex.lock ();
        mt_unlocks (mutex) completeHostnameLookup (&addr);
    }

    return Result::Success;
}

RealtimeHlsClient::RealtimeHlsClient (EmbedContainer * const embed_container)
    : VodSource         (embed_container),
      http_client       (/*embed_container=*/ this),
      port              (80),
      hostname_resolved (false),
      start_pending     (false),
      stopped           (false)
{
}

}

