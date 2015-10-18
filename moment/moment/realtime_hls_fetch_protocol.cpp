/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/realtime_hls_fetch_protocol.h>


namespace Moment {

VodSource::Frontend const RealtimeHlsFetchConnection::vod_source_frontend = {
    mediaMessage,
    endOfSource
};

void
RealtimeHlsFetchConnection::mediaMessage (MediaMessage * const mt_nonnull msg,
                                          void         * const _self)
{
}

void
RealtimeHlsFetchConnection::endOfSource (VodSource::EndReason   const end_reason,
                                         void                 * const _self)
{
}

Result
RealtimeHlsFetchConnection::start ()
{
    logD_ (_this_func_);

//    client->start ();

    return Result::Success;
}

mt_const void
RealtimeHlsFetchConnection::init (ServerThreadContext * const mt_nonnull thread_ctx,
                                  PagePool            * const mt_nonnull page_pool,
                                  MediaStream         * const mt_nonnull stream,
                                  ConstMemory           const uri,
                                  CbDesc<FetchConnection::FetchConnectionFrontend> const &frontend)
{
    logD_ (_this_func_);

    this->frontend = frontend;
    this->stream = stream;

    // TODO
}

Ref<FetchConnection>
RealtimeHlsFetchProtocol::newConnection (ChannelOptions * const channel_opts,
                                         PlaybackItem   * const playback_item,
                                         VideoStream    * const stream,
                                         ConstMemory     uri,
                                         CbDesc<FetchConnection::FetchConnectionFrontend> const &frontend)
{
#if 0
    Ref<RtspFetchConnection> const rtsp_fetch_conn = grab (new (std::nothrow) RtspFetchConnection (NULL /* embed_container */));

    IpAddress   server_addr;
    ConstMemory app_name;
    ConstMemory stream_name;
    bool        momentrtmp_proto = false;
    ConstMemory login;
    ConstMemory password;
    Size        loginpass_beg = 0;
    Size        loginpass_end = 0;
    if (!parseMomentUri (uri,
                         554 /* default_port */,
                         &server_addr,
                         &app_name,
                         &stream_name,
                         &momentrtmp_proto,
                         &login,
                         &password,
                         NULL /* ret_proto */,
                         &loginpass_beg,
                         &loginpass_end))
    {
        logE_ (_func, "Could not parse uri: ", uri);
        return NULL;
    }

    if (login.mem() || password.mem())
        logD_ (_func, "login: ", login, ", password: ", password);

    logD_ (_func, "server_addr: ", server_addr, ", "
           "app_name: ", app_name, ", stream_name: ", stream_name, ", "
           "momentrtmp_proto: ", momentrtmp_proto);

    StRef<String> uri_no_loginpass;
    if (loginpass_beg && loginpass_end) {
        uri_no_loginpass = makeString (uri.region (0, loginpass_beg), uri.region (loginpass_end));
        uri = uri_no_loginpass;
    }

    bool rtsp_tcp_first = tcp_interleaved;
    {
        // TODO Add the following options:
        //          rtsp_timeout = no_video_timeout
        //          rtsp_udp = y
        //          rtsp_tcp = y
        //          rtsp_tcp_first = n

        if (playback_item->input_opts) {
            if (MConfig::Section * const input_section = playback_item->input_opts->config_section) {
                if (!configSectionGetBoolean_ (input_section, "rtsp_tcp_first", &rtsp_tcp_first, _func))
                    return NULL;
            }
        }
    }

    rtsp_fetch_conn->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                           moment->getPagePool(),
                           stream,
                           server_addr,
                           uri,
                           login,
                           password,
                           rtsp_tcp_first,
                           !channel_opts->default_item->no_audio && !playback_item->no_audio,
                           !channel_opts->default_item->no_video && !playback_item->no_video,
                           channel_opts->no_video_timeout * 1000 / 10 /* TODO check_interval_millisec */,
                           channel_opts->no_video_timeout * 1000,
                           playback_item->drop_sei_nal_units,
                           frontend);

    return rtsp_fetch_conn;
#endif

    return NULL;
}

mt_const Result
RealtimeHlsFetchProtocol::init (MomentServer * const mt_nonnull moment)
{
    this->moment = moment;

    return Result::Success;
}

RealtimeHlsFetchProtocol::RealtimeHlsFetchProtocol (EmbedContainer * const embed_container)
    : FetchProtocol (embed_container)
{
}

}

