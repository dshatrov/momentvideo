/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_RTSP__RTSP_FETCH_PROTOCOL__H__
#define MOMENT_RTSP__RTSP_FETCH_PROTOCOL__H__


#include <moment/moment_server.h>
#include <moment-rtsp/rtsp_client.h>


namespace MomentRtsp {

class RtspFetchProtocol : public FetchProtocol
{
  private:
    mt_const Ref<MomentServer> moment;

    mt_const bool tcp_interleaved;

  public:
    mt_iface (FetchProtocol)
      Ref<FetchConnection> newConnection (ChannelOptions * mt_nonnull channel_opts,
                                          PlaybackItem   * mt_nonnull playback_item,
                                          VideoStream    *stream,
                                          ConstMemory     uri,
                                          CbDesc<FetchConnection::FetchConnectionFrontend> const
                                                         &fetch_connection_frontend);
    mt_iface_end

    mt_const Result init (MomentServer * mt_nonnull moment);

    RtspFetchProtocol (EmbedContainer *embed_container);
};

}


#endif /* MOMENT_RTSP__RTSP_FETCH_PROTOCOL__H__ */

