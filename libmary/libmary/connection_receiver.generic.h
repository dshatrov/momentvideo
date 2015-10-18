/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__CONNECTION_RECEIVER__GENERIC__H__
#define LIBMARY__CONNECTION_RECEIVER__GENERIC__H__


#include <libmary/receiver.h>
#include <libmary/async_input_stream.h>


namespace M {


// TODO Rename to AsyncStreamReceiver. It now depends on AsyncInputStream, not on Connection.

// TODO Input blocking logics - ?

// TODO Configurable recv_buf_len (init() param)

// Synchronized externally by the associated AsyncInputStream object.
//
class ConnectionReceiver : public Receiver
{
  private:
    DeferredProcessor::Task unblock_input_task;
    DeferredProcessor::Registration deferred_reg;

    mt_const Ref<AsyncInputStream> conn;
    mt_const bool use_recvfrom;

    mt_const Byte *recv_buf;
    mt_const Size const recv_buf_len;

    mt_sync_domain (conn_input_frontend) IpAddress from_addr;

    mt_sync_domain (conn_input_frontend) Size recv_buf_pos;
    mt_sync_domain (conn_input_frontend) Size recv_accepted_pos;

    mt_sync_domain (conn_input_frontend) bool block_input;
    mt_sync_domain (conn_input_frontend) bool error_received;
    mt_sync_domain (conn_input_frontend) bool error_reported;

    mt_sync_domain (conn_input_frontend) void doProcessInput ();

    mt_iface (AsyncInputStream::InputFrontend)
      static AsyncInputStream::InputFrontend const conn_input_frontend;

      static mt_sync_domain (conn_input_frontend) void processInput (void *_self);

      static mt_sync_domain (conn_input_frontend) void processError (Exception *exc_,
                                                                     void      *_self);
    mt_iface_end

    mt_sync_domain (conn_input_frontend) void doUnblockInput ();

    static mt_sync_domain (conn_input_frontend) bool unblockInputTask (void *_self);

  public:
    mt_iface (Receiver)
      void unblockInput ();
    mt_iface_end

    void start ();

    mt_const void init (AsyncInputStream  * mt_nonnull conn,
                        DeferredProcessor *deferred_processor,
                        bool               block_input  = false,
                        bool               use_recvfrom = false);

     ConnectionReceiver (EmbedContainer *embed_container);
    ~ConnectionReceiver ();
};

typedef ConnectionReceiver AsyncStreamReceiver;

}


#endif /* LIBMARY__CONNECTION_RECEIVER__GENERIC__H__ */

