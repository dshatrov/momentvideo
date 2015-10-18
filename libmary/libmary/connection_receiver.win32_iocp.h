/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__CONNECTION_RECEIVER__WIN32_IOCP__H__
#define LIBMARY__CONNECTION_RECEIVER__WIN32_IOCP__H__


#include <libmary/receiver.h>
#include <libmary/async_input_stream.h>
#include <libmary/iocp_poll_group.h>


namespace M {

class ConnectionReceiver : public Receiver
{
private:
    struct ReceiverOverlapped : public Overlapped
    {
        Byte *recv_buf;
        ~ReceiverOverlapped ();
    };

    DeferredProcessor::Task start_task;
    DeferredProcessor::Task unblock_input_task;
    DeferredProcessor::Registration deferred_reg;

    mt_const Ref<ReceiverOverlapped> receiver_overlapped;
    mt_const Ref<AsyncInputStream> conn;
    mt_const Size const recv_buf_len;

    mt_sync_domain (inputIoComplete) Size recv_buf_pos;
    mt_sync_domain (inputIoComplete) Size recv_accepted_pos;
    mt_sync_domain (inputIoComplete) Size toread;
    mt_sync_domain (inputIoComplete) bool overlapped_pending;

    mt_sync_domain (inputIoComplete) void initiateReceive ();

    mt_sync_domain (inputIoComplete) void doProcessInput ();

    mt_sync_domain (inputIoComplete) void doStart ();

    static mt_sync_domain (inputIoComplete) bool startTask (void *_self);
    static mt_sync_domain (inputIoComplete) bool unblockInputTask (void *_self);

    static mt_sync_domain (inputIoComplete) void inputIoComplete (Exception  *exc_,
                                                                  Overlapped * mt_nonnull overlapped,
                                                                  Size        bytes_transferred,
                                                                  void       *_self);

public:
  mt_iface (Receiver)
    void unblockInput ();
  mt_iface_end

    void start ();

    mt_const void init (AsyncInputStream  * mt_nonnull conn,
                        DeferredProcessor *deferred_processor,
                        bool               block_input = false);

     ConnectionReceiver (EmbedContainer *embed_container);
    ~ConnectionReceiver ();
};

typedef ConnectionReceiver AsyncStreamReceiver;

}


#endif /* LIBMARY__CONNECTION_RECEIVER__WIN32_IOCP__H__ */

