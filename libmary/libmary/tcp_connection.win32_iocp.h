/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__TCP_CONNECTION__WIN32_IOCP__H__
#define LIBMARY__TCP_CONNECTION__WIN32_IOCP__H__


#include <libmary/connection.h>
#include <libmary/poll_group.h>
#include <libmary/exception.h>
#include <libmary/util_net.h>


namespace M {

class TcpConnection : public Connection
{
public:
    struct Frontend {
	void (*connected) (Exception *exc_,
			   void      *cb_data);
    };

private:
    mt_const Cb<Frontend> frontend;
    mt_const Ref<Overlapped> connect_overlapped;

    mt_const SOCKET socket_fd;

    mt_sync_domain (connectIoComplete) bool connected;

  mt_iface (PollGroup::Pollable)
    static PollGroup::Pollable const pollable;
    static SOCKET getFd (void *_self);
  mt_iface_end

    static void connectIoComplete (Exception  *exc_,
                                   Overlapped * mt_nonnull overlapped,
                                   Size        bytes_transferred,
                                   void       *_self);

public:
  mt_iface (Connection)
    mt_iface (AsyncInputStream)
      mt_throws AsyncIoResult read (OVERLAPPED * mt_nonnull overlapped,
                                    Memory      mem,
                                    Size       *ret_nread);
    mt_iface_end

    mt_iface (AsyncOutputStream)
      // Note: these are mostly useless wrappers around WSASend().
      mt_throws AsyncIoResult write (OVERLAPPED  * mt_nonnull overlapped,
                                     ConstMemory  mem,
                                     Size        *ret_nwritten);

      mt_throws AsyncIoResult writev (OVERLAPPED  * mt_nonnull overlapped,
                                      WSABUF      * mt_nonnull buffers,
                                      Count        buffer_count,
                                      Size        *ret_nwritten);
    mt_iface_end
  mt_iface_end

    mt_throws Result getLocalAddress (IpAddress * const mt_nonnull ret_addr);

    mt_const mt_throws Result open ();

    enum ConnectResult
    {
        ConnectResult_Error = 0,
        ConnectResult_Connected,
        ConnectResult_InProgress
    };

    // May be called only once.
    mt_const mt_throws ConnectResult connect (IpAddress addr);

    // Should be called just once by TcpServer.
    mt_const void setSocket (SOCKET const socket_fd)
    {
        this->socket_fd = socket_fd;
        connected = true;
    }

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    mt_const void setFrontend (Cb<Frontend> const &frontend)
        { this->frontend = frontend; }

     TcpConnection (EmbedContainer *embed_container);
    ~TcpConnection ();
};

}


#endif /* LIBMARY__TCP_CONNECTION__WIN32_IOCP__H__ */

