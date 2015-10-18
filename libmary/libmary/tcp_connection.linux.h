/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__TCP_CONNECTION__LINUX__H__
#define LIBMARY__TCP_CONNECTION__LINUX__H__


#include <libmary/connection.h>
#include <libmary/poll_group.h>
#include <libmary/util_net.h>
#include <libmary/debug.h>


namespace M {

class TcpConnection : public Connection
{
  public:
    struct Frontend {
        void (*connected) (Exception *exc_,
                           void      *cb_data);
    };

    #ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
      static AtomicInt num_instances;
    #endif

  private:
    mt_const int fd;

    mt_sync_domain (pollable) bool connected;

    // Synchronized by processEvents() and also used by read(). This implies
    // that read() must be called from the same thread as processEvents(),
    // which is not very pleasant.
    bool hup_received;

    Cb<Frontend> frontend;
    Cb<PollGroup::Feedback> feedback;

    void requestInput ()
    {
        if (feedback && feedback->requestInput)
            feedback.call (feedback->requestInput);
    }

    void requestOutput ()
    {
        if (feedback && feedback->requestOutput)
            feedback.call (feedback->requestOutput);
    }

    mt_iface (PollGroup::Pollable)
      static PollGroup::Pollable const pollable;

      static void processEvents (Uint32  event_flags,
                                 void   *_self);

      static int getFd (void *_self);

      static void setFeedback (Cb<PollGroup::Feedback> const &feedback,
                               void *_self);
    mt_iface_end

  public:
    mt_iface (Connection)
      mt_iface (AsyncInputStream)
        mt_throws AsyncIoResult read (Memory  mem,
                                      Size   *ret_nread);

        mt_throws AsyncIoResult recvFrom (Memory     mem,
                                          Size      *ret_nread,
                                          IpAddress *ret_addr);
      mt_iface_end

      mt_iface (AsyncOutputStream)
        mt_throws AsyncIoResult write (ConstMemory  mem,
                                       Size        *ret_nwritten);

        mt_throws AsyncIoResult writev (struct iovec *iovs,
                                        Count         num_iovs,
                                        Size         *ret_nwritten);
      mt_iface_end

      #ifdef LIBMARY_ENABLE_MWRITEV
        int getFd () { return fd; }
      #endif
    mt_iface_end

    mt_throws Result getLocalAddress (IpAddress * mt_nonnull ret_addr);

    void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    mt_const mt_throws Result open ();

    enum ConnectResult
    {
        ConnectResult_Error = 0,
        ConnectResult_Connected,
        ConnectResult_InProgress
    };

    // May be called only once. Must be called early (during initialzation)
    // to ensure proper synchronization of accesses to 'connected' data member.
    mt_const mt_throws ConnectResult connect (IpAddress addr);

    // Should be called just once by TcpServer.
    mt_const void setFd (int const fd)
    {
        this->fd = fd;
        connected = true;
    }

     TcpConnection (EmbedContainer *embed_container);
    ~TcpConnection ();
};

}


#endif /* LIBMARY__TCP_CONNECTION__LINUX__H__ */

