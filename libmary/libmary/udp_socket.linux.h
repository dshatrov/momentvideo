/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UDP_SOCKET__LINUX__H__
#define LIBMARY__UDP_SOCKET__LINUX__H__


#include <libmary/common_socket_posix.h>
#include <libmary/poll_group.h>
#include <libmary/util_net.h>


namespace M {

class UdpSocket : public CommonSocket
{
  private:
    mt_iface (PollGroup::Pollable)
      static PollGroup::Pollable const pollable;

      static void processEvents (Uint32  event_flags,
                                 void   *_self);

      static void setFeedback (Cb<PollGroup::Feedback> const &feedback,
                               void *_self);

      static int getFd (void *_self);
    mt_iface_end

  public:
    mt_iface (Connection)
      AsyncIoResult sendVectorTo (IpAddress     addr,
                                  struct iovec * mt_nonnull iovs,
                                  Count         num_iovs,
                                  Size         *ret_nwritten);
    mt_iface_end

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    mt_const mt_throws Result open ();

    mt_const mt_throws Result connect (IpAddress addr);

    mt_const mt_throws Result bind (IpAddress addr);

    mt_const mt_throws Result bindToAnyLocalPort ();

     UdpSocket (EmbedContainer *embed_container);
    ~UdpSocket ();
};

}


#endif /* LIBMARY__UDP_SOCKET__LINUX__H__ */

