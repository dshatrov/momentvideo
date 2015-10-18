/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__TCP_CONNECTION__WIN32__H__
#define LIBMARY__TCP_CONNECTION__WIN32__H__


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
			   void *cb_data);
    };

    #ifdef LIBMARY_TCP_CONNECTION_NUM_INSTANCES
      static AtomicInt num_instances;
    #endif

  private:
    mt_const SOCKET fd;

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

      static void setFeedback (Cb<PollGroup::Feedback> const &feedback,
                               void *_self);

      static SOCKET getFd (void *_self);
    mt_iface_end

  public:
    mt_iface (Connection)
      mt_iface (AsyncInputStream)
        mt_throws AsyncIoResult read (Memory  mem,
                                      Size   *ret_nread);
      mt_iface_end

      mt_iface (AsyncOutputStream)
        mt_throws AsyncIoResult write (ConstMemory  mem,
                                       Size        *ret_nwritten);

        /* This may be implemented one day.
        mt_throws AsyncIoResult writev (struct iovec *iovs,
                                        Count         num_iovs,
                                        Size         *ret_nwritten);
        */
      mt_iface_end
    mt_iface_end

    // Note that close() closes the file descriptor, which may cause races
    // if the connection object is still in use, i.e. it is referenced and
    // read/write methods may potentially be called.
  private:
    mt_throws Result doClose ();

  public:
    void setFrontend (Cb<Frontend> const &frontend)
        { this->frontend = frontend; }

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    mt_throws Result getLocalAddress (IpAddress * const mt_nonnull ret_addr)
    {
//#warning TODO
        return Result::Failure;
    }

    mt_throws Result open ();

    enum ConnectResult
    {
        ConnectResult_Error = 0,
        ConnectResult_Connected,
        ConnectResult_InProgress
    };

    // May be called only once. Must be called early (during initialzation)
    // to ensure proper synchronization of accesses to 'connected' data member.
    mt_throws ConnectResult connect (IpAddress addr);

    // Should be called just once by TcpServer.
    mt_const void setFd (SOCKET const fd)
    {
	this->fd = fd;
	connected = true;
    }

     TcpConnection (EmbedContainer *embed_container);
    ~TcpConnection ();
};

}


#endif /* LIBMARY__TCP_CONNECTION__WIN32__H__ */

