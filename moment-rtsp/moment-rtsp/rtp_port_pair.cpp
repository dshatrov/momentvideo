/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-rtsp/rtp_port_pair.h>


namespace MomentRtsp {
    
static Uint16 normalizePortNumber (Uint16 const port)
{
    // RFC 3551 suggests that we use port numbers above 5000.
    if (port >= 65534)
        return 5002;

    if (port < 5002)
        return 5002;

    return port;
}

Result
RtpPortPair::openAndBindSocketPair (UdpSocket * const mt_nonnull rtp_socket,
                                    UdpSocket * const mt_nonnull rtcp_socket,
                                    Uint16    * const mt_nonnull ret_rtp_port,
                                    Uint16    * const mt_nonnull ret_rtcp_port)
{
    port_number_mutex.lock ();

    if (port_number == 0)
        port_number = normalizePortNumber (randomUint32() % (65533 - 5002) / 2 * 2 + 5002);

    Uint16 const first_port_number = port_number;
    unsigned iteration = 1;
    for (;;) {
        bool ok = false;
        do {
            {
                IpAddress rtp_addr;
                if (!setIpAddress (ConstMemory(), port_number, &rtp_addr)) {
                    logE_ (_this_func, "setIpAddress() failed");
                    port_number_mutex.unlock ();
                    return Result::Failure;
                }
                *ret_rtp_port = port_number;

                if (!rtp_socket->open ()) {
                    logE_ (_this_func, "rtp_socket->open() failed: ", exc->toString());
                    port_number_mutex.unlock ();
                    return Result::Failure;
                }

                if (!rtp_socket->bind (rtp_addr)) {
                    logD_ (_func, "bind (", rtp_addr, ") failed");
                    break;
                }
            }

            {
                IpAddress rtcp_addr;
                if (!setIpAddress (ConstMemory(), port_number + 1, &rtcp_addr)) {
                    logE_ (_this_func, "setIpAddress() failed");
                    port_number_mutex.unlock ();
                    return Result::Failure;
                }
                *ret_rtcp_port = port_number + 1;

                if (!rtcp_socket->open ()) {
                    logE_ (_this_func, "rtcp_socket->open() failed: ", exc->toString());
                    port_number_mutex.unlock ();
                    return Result::Failure;
                }

                if (!rtcp_socket->bind (rtcp_addr)) {
                    logD_ (_func, "bind (", rtcp_addr, ") failed");
                    break;
                }
            }

            ok = true;
        } while (0);

        port_number = normalizePortNumber (port_number + 2);

        if (ok)
            break;

        if (port_number == first_port_number) {
            if (iteration >= 3) {
                logF_ (_this_func, "Could not find any spare UDP port number pair for RTP+RTCP");
                port_number_mutex.unlock ();
                return Result::Failure;
            }

            ++iteration;
        }
    }
    port_number_mutex.unlock ();

    return Result::Success;
}

RtpPortPair::RtpPortPair ()
    : port_number (0)
{
}

}

