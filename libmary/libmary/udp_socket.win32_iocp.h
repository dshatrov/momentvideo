/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UDP_SOCKET__WIN32_IOCP__H__
#define LIBMARY__UDP_SOCKET__WIN32_IOCP__H__


#include <libmary/common_socket.win32_iocp.h>
#include <libmary/poll_group.h>
#include <libmary/util_net.h>


namespace M {

class UdpSocket : public CommonSocket
{
private:
  mt_iface (PollGroup::Pollable)
    static PollGroup::Pollable const pollable;
    static SOCKET getFd (void *_self);
  mt_iface_end

public:
  mt_iface (Connection)
    AsyncIoResult sendVectorTo (IpAddress    addr,
                                OVERLAPPED * mt_nonnull overlapped,
                                WSABUF     * mt_nonnull buffers,
                                Count        buf_count,
                                Size        *ret_nwritten);
  mt_iface_end

    CbDesc<PollGroup::Pollable> getPollable ()
       { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    mt_const mt_throws Result open ();

#if 0
// Unused
    mt_const mt_throws Result connect (IpAddress addr);
#endif

    mt_const mt_throws Result bind (IpAddress addr);

#if 0
// Unused
    mt_const mt_throws Result bindToAnyLocalPort ();
#endif

     UdpSocket (EmbedContainer *embed_container);
    ~UdpSocket ();
};

}


#endif /* LIBMARY__UDP_SOCKET__WIN32_IOCP__H__ */

