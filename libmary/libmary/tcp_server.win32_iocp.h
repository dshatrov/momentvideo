/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__TCP_SERVER__WIN32_IOCP__H__
#define LIBMARY__TCP_SERVER__WIN32_IOCP__H__


#include <libmary/types.h>
#include <mswsock.h>

#include <libmary/timers.h>
#include <libmary/poll_group.h>
#include <libmary/exception.h>
#include <libmary/tcp_connection.h>


namespace M {

class TcpServer : public Object
{
public:
    struct Frontend
    {
        void (*accepted) (void *cb_data);
    };

private:
    mt_const Cb<Frontend> frontend;

    struct ServerOverlapped : public Overlapped
    {
        char out_buf [(sizeof (sockaddr_in) + 16) * 2];
    };

    mt_const Ref<ServerOverlapped> server_overlapped;

    mt_const SOCKET socket_fd;
    mt_const LPFN_ACCEPTEX ptr_AcceptEx;
    mt_const LPFN_GETACCEPTEXSOCKADDRS ptr_GetAcceptExSockaddrs;

    DeferredProcessor::Task start_task;
    DeferredProcessor::Registration deferred_reg;

    mt_sync_domain (acceptIoComplete) SOCKET conn_socket;
    mt_sync_domain (acceptIoComplete) bool accepted;

    mt_sync_domain (acceptIoComplete) mt_throws Result initiateAccept ();

  mt_iface (PollGroup::Pollable)
    static PollGroup::Pollable const pollable;
    static SOCKET getFd (void *_self);
  mt_iface_end

    static bool startTask (void *_self);

    static void acceptIoComplete (Exception  *exc_,
                                  Overlapped * mt_nonnull overlapped,
                                  Size        bytes_transferred,
                                  void       *_self);

public:
    mt_throws Result open ();

    enum_beg (AcceptResult)
        enum_values (Accepted, NotAccepted, Error)
    enum_end (AcceptResult)

    mt_sync_domain (acceptIoComplete)
        mt_throws AcceptResult accept (TcpConnection * mt_nonnull tcp_conn,
                                       IpAddress     *ret_addr = NULL);

    mt_throws Result bind (IpAddress addr);

    mt_throws Result listen ();

    // Should be called after addPollable().
    mt_throws Result start ();

    CbDesc<PollGroup::Pollable> getPollable ()
        { return CbDesc<PollGroup::Pollable> (&pollable, this, this); }

    void init (CbDesc<Frontend> const &frontend,
               DeferredProcessor * mt_nonnull deferred_processor,
               Timers            * mt_nonnull timers,
               Time               accept_retry_timeout_millisec = 1000);

     TcpServer (EmbedContainer *embed_container);
    ~TcpServer ();
};

}


#endif /* LIBMARY__TCP_SERVER__WIN32_IOCP__H__ */

