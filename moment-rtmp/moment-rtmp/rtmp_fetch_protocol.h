/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__RTMP_FETCH_PROTOCOL__H__
#define MOMENT__RTMP_FETCH_PROTOCOL__H__


#include <libmary/libmary.h>
#include <moment/fetch_protocol.h>
#include <moment/moment_server.h>

#include <moment-rtmp/rtmp_client.h>


namespace Moment {

using namespace M;

class RtmpFetchProtocol : public FetchProtocol
{
private:
    mt_const Ref<MomentServer> moment;
    mt_const Time ping_timeout_millisec;
    mt_const Time no_video_check_interval_millisec;
    mt_const Time no_video_timeout_millisec;

public:
  mt_iface (FetchProtocol)
    Ref<FetchConnection> newConnection (ChannelOptions * mt_nonnull channel_opts,
                                        PlaybackItem   * mt_nonnull playback_item,
                                        VideoStream    *stream,
                                        ConstMemory     uri,
                                        CbDesc<FetchConnection::FetchConnectionFrontend> const &frontend);
  mt_iface_end

    mt_const void init (MomentServer * mt_nonnull moment,
                        Time          ping_timeout_millisec);

    RtmpFetchProtocol (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__RTMP_FETCH_PROTOCOL__H__ */

