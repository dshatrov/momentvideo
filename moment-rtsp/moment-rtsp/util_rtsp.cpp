/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-rtsp/util_rtsp.h>


namespace MomentRtsp {

static void parsePortPair (ConstMemory   const param_name,
                           ConstMemory   const value,
                           Uint32      * const mt_nonnull ret_port_rtp,
                           Uint32      * const mt_nonnull ret_port_rtcp)
{
    logD_ (_func, param_name, ": ", value);

    *ret_port_rtp  = 0;
    *ret_port_rtcp = 0;

    Size j = 0;
    for (;;) {
        if (j >= value.len() || value.buf() [j] == '-') {
            break;
        }
        ++j;
    }

    ConstMemory const rtp_port_mem  = value.region (0, j);
    ConstMemory const rtcp_port_mem =
            (j < value.len() ? value.region (j + 1) : ConstMemory());

    if (rtp_port_mem.len()) {
        if (!strToUint32 (rtp_port_mem, ret_port_rtp, NULL, 10)) {
            logW_ (_func, "bad ", param_name, " RTP port value: ", rtp_port_mem);
            *ret_port_rtp = 0;
        }
    }

    if (rtcp_port_mem.len()) {
        if (!strToUint32 (rtcp_port_mem, ret_port_rtcp, NULL, 10)) {
            logW_ (_func, "bad ", param_name, " RTCP channel value: ", rtp_port_mem);
            *ret_port_rtcp = 0;
        }
    }
}

void parseRtspTransportHeader (HttpRequest             * const mt_nonnull req,
                               RtspTransportHeaderInfo * const mt_nonnull ret_transport_info)
{
    ret_transport_info->reset ();

    ConstMemory const transport = req->getHeader ("transport");

    Size i = 0;
    while (i < transport.len() && (transport.mem() [i] == ' ' || transport.mem() [i] == '\t'))
        ++i;

    Size param_beg = i;
    while (i < transport.len() + 1) {
        if (i == transport.len()
            || transport.mem() [i] == ';' || transport.mem() [i] == '\r' || transport.mem() [i] == '\n')
        {
            ConstMemory const param = ConstMemory (transport.mem() + param_beg, i - param_beg);
            logD_ (_func, "param: ", param);

            ConstMemory client_port_name ("client_port=");
            ConstMemory server_port_name ("server_port=");
            ConstMemory interleaved_name ("interleaved=");

            if (param.len() >= client_port_name.len()
                && equal (param.region (0, client_port_name.len()), client_port_name))
            {
                ret_transport_info->got_client_port = true;
                parsePortPair ("client_port",
                               param.region (client_port_name.len()),
                               &ret_transport_info->client_port_rtp,
                               &ret_transport_info->client_port_rtcp);
            } else
            if (param.len() >= server_port_name.len()
                && equal (param.region (0, server_port_name.len()), server_port_name))
            {
                ret_transport_info->got_server_port = true;
                parsePortPair ("server_port",
                               param.region (server_port_name.len()),
                               &ret_transport_info->server_port_rtp,
                               &ret_transport_info->server_port_rtcp);
            } else
            if (param.len() >= interleaved_name.len()
                && equal (param.region (0, interleaved_name.len()), interleaved_name))
            {
                ret_transport_info->is_tcp_interleaved = true;
                parsePortPair ("interleaved",
                               param.region (interleaved_name.len()),
                               &ret_transport_info->tcp_channel_rtp,
                               &ret_transport_info->tcp_channel_rtcp);
            }

            param_beg = i + 1;
        }

        ++i;
    }
}

}

