/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_config.h>

#include <moment-rtsp/rtsp_fetch_protocol.h>


namespace MomentRtsp {

Ref<FetchConnection>
RtspFetchProtocol::newConnection (ChannelOptions * const channel_opts,
                                  PlaybackItem   * const playback_item,
                                  VideoStream    * const stream,
                                  ConstMemory      const uri,
                                  CbDesc<FetchConnection::FetchConnectionFrontend> const &fetch_connection_frontend)
{
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

    RtspClientConfig rtsp_client_config;
    rtsp_client_config.tcp_interleaved                   = rtsp_tcp_first;
    rtsp_client_config.enable_audio                      = !channel_opts->default_item->no_audio && !playback_item->no_audio;
    rtsp_client_config.enable_video                      = !channel_opts->default_item->no_video && !playback_item->no_video;
    rtsp_client_config.no_avdata_check_interval_millisec = channel_opts->no_video_timeout * 1000 / 10 /* TODO check_interval_millisec */;
    rtsp_client_config.no_avdata_timeout_millisec        = channel_opts->no_video_timeout * 1000;
    rtsp_client_config.drop_sei_nal_units                = playback_item->drop_sei_nal_units;

    Ref<RtspClient> const rtsp_client = grabNewObject <RtspClient> ();

    if (!rtsp_client->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                            moment->getPagePool(),
                            uri,
                            &rtsp_client_config,
                            stream,
                            fetch_connection_frontend))
    {
        logE_ (_this_func, "rtsp_client.init() failed");
        return NULL;
    }

    return rtsp_client;
}

mt_const Result
RtspFetchProtocol::init (MomentServer * const mt_nonnull moment)
{
    this->moment = moment;

    Ref<MConfig::Config> const config = moment->getConfigManager()->getConfig();

    if (!configGetBoolean_ (config, "mod_rtsp/fetch_tcp_interleaved", &tcp_interleaved, _func))
        return Result::Failure;

    return Result::Success;
}

RtspFetchProtocol::RtspFetchProtocol (EmbedContainer * const embed_container)
    : FetchProtocol (embed_container),
      tcp_interleaved (false)
{
}

}

