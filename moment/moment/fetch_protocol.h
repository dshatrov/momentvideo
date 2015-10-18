/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__FETCH_PROTOCOL__H__
#define MOMENT__FETCH_PROTOCOL__H__


#include <libmary/libmary.h>

#include <moment/channel_options.h>
#include <moment/stream.h>
#include <moment/fetch_connection.h>


namespace Moment {

class FetchProtocol : public Object
{
  public:
    virtual Ref<FetchConnection> newConnection (ChannelOptions * mt_nonnull channel_opts,
                                                PlaybackItem   * mt_nonnull playback_item,
                                                VideoStream    *stream,
                                                ConstMemory     uri,
                                                CbDesc<FetchConnection::FetchConnectionFrontend> const
                                                               &fetch_connection_frontend) = 0;

    FetchProtocol (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__FETCH_PROTOCOL__H__ */

