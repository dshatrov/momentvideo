/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>


namespace MomentRtsp {

using namespace M;

struct RtspTransportHeaderInfo
{
    bool got_client_port;
    Uint32 client_port_rtp;
    Uint32 client_port_rtcp;

    bool got_server_port;
    Uint32 server_port_rtp;
    Uint32 server_port_rtcp;

    bool is_tcp_interleaved;
    Uint32 tcp_channel_rtp;
    Uint32 tcp_channel_rtcp;

    void reset ()
    {
        got_client_port  = false;
        client_port_rtp  = 0;
        client_port_rtcp = 0;

        got_server_port  = false;
        server_port_rtp  = 0;
        server_port_rtcp = 0;

        is_tcp_interleaved = false;
        tcp_channel_rtp    = 0;
        tcp_channel_rtcp   = 0;
    }

    RtspTransportHeaderInfo ()
    {
        reset ();
    }
};

void parseRtspTransportHeader (HttpRequest             * mt_nonnull req,
                               RtspTransportHeaderInfo * mt_nonnull ret_header_info);

}

