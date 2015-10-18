/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>


namespace MomentRtsp {

using namespace M;

class RtpPortPair
{
  private:
    Mutex port_number_mutex;
    mt_mutex (port_number_mutex) Uint16 port_number;

  public:
    Result openAndBindSocketPair (UdpSocket * mt_nonnull rtp_socket,
                                  UdpSocket * mt_nonnull rtcp_socket,
                                  Uint16    * mt_nonnull ret_rtp_port,
                                  Uint16    * mt_nonnull ret_rtcp_port);

    RtpPortPair ();
};

}

