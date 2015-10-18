/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__REALTIME_HLS_FETCH_PROTOCOL__H__
#define MOMENT__REALTIME_HLS_FETCH_PROTOCOL__H__


#include <moment/fetch_protocol.h>
#include <moment/realtime_hls_client.h>
#include <moment/moment_server.h>


namespace Moment {

class RealtimeHlsFetchConnection : public FetchConnection
{
  private:
    Embed<RealtimeHlsClient> realtime_hls_client;

    mt_const Cb<FetchConnection::FetchConnectionFrontend> frontend;
    mt_const Ref<MediaStream> stream;

    mt_iface (VodSource::Frontend)
      static VodSource::Frontend const vod_source_frontend;

      static void mediaMessage (MediaMessage * mt_nonnull msg,
                                void         *_self);

      static void endOfSource (VodSource::EndReason  end_reason,
                               void                 *_self);
    mt_iface_end

  public:
    mt_iface (FetchConnection)
      Result start ();
    mt_iface_end

    mt_const void init (ServerThreadContext * mt_nonnull thread_ctx,
                        PagePool            * mt_nonnull page_pool,
                        MediaStream         * mt_nonnull stream,
                        ConstMemory          uri,
                        CbDesc<FetchConnection::FetchConnectionFrontend> const &frontend);

    RealtimeHlsFetchConnection (EmbedContainer * const embed_container)
        : FetchConnection     (embed_container),
          realtime_hls_client (this /* embed_container */)
    {}
};

class RealtimeHlsFetchProtocol : public FetchProtocol
{
  private:
    mt_const Ref<MomentServer> moment;

  public:
    mt_iface (FetchProtocol)
      Ref<FetchConnection> newConnection (ChannelOptions * mt_nonnull channel_opts,
                                          PlaybackItem   * mt_nonnull playback_item,
                                          MediaStream    *stream,
                                          ConstMemory     uri,
                                          CbDesc<FetchConnection::FetchConnectionFrontend> const &frontend);
    mt_iface_end

    mt_const Result init (MomentServer * mt_nonnull moment);

    RealtimeHlsFetchProtocol (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__REALTIME_HLS_FETCH_PROTOCOL__H__ */

