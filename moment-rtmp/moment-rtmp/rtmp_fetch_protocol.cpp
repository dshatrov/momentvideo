/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_moment.h>

#include <moment-rtmp/rtmp_fetch_protocol.h>


using namespace M;

namespace Moment {

Ref<FetchConnection>
RtmpFetchProtocol::newConnection (ChannelOptions * const mt_nonnull channel_opts,
                                  PlaybackItem   * const mt_nonnull /* playback_item */,
                                  VideoStream    * const stream,
                                  ConstMemory      const uri,
                                  CbDesc<FetchConnection::FetchConnectionFrontend> const &frontend)
{
    IpAddress   server_addr;
    ConstMemory app_name;
    ConstMemory stream_name;
    bool        momentrtmp_proto;
    if (!parseMomentUri (uri, 1935 /* default_port */, &server_addr, &app_name, &stream_name, &momentrtmp_proto)) {
        logE_ (_func, "Could not parse uri: ", uri);
        return NULL;
    }

    logD_ (_func, "server_addr: ", server_addr, ", "
           "app_name: ", app_name, ", stream_name: ", stream_name, ", "
           "momentrtmp_proto: ", momentrtmp_proto, ", ping_timeout_millisec: ", ping_timeout_millisec);

    Ref<RtmpClient> const rtmp_client = grabNewObject <RtmpClient> ();

    rtmp_client->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                       moment->getPagePool(),
                       stream,
                       server_addr,
                       app_name,
                       stream_name,
                       RtmpClient::RtmpClientConfig (
                           momentrtmp_proto,
                           ping_timeout_millisec,
                           0 /* send_delay_millisec */,
                           channel_opts->no_video_timeout * 1000 / 10 /* TODO check_interval_millisec */,
                           channel_opts->no_video_timeout * 1000),
                       frontend);

    return rtmp_client;
}

mt_const void
RtmpFetchProtocol::init (MomentServer * const mt_nonnull moment,
                         Time           const ping_timeout_millisec)
{
    this->moment = moment;
    this->ping_timeout_millisec = ping_timeout_millisec;
}

RtmpFetchProtocol::RtmpFetchProtocol (EmbedContainer * const embed_container)
    : FetchProtocol (embed_container),
      ping_timeout_millisec (0)
{}

}

