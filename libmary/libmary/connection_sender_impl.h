/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__CONNECTION_SENDER_IMPL__H__
#define LIBMARY__CONNECTION_SENDER_IMPL__H__


#include <libmary/types_base.h>
#ifdef LIBMARY_WIN32_IOCP
  #include <libmary/iocp_poll_group.h>
#endif

#include <libmary/async_output_stream.h>
#include <libmary/sender.h>


// TODO ConnectionReady notification is delivered too early, even when the queue is full
//      and the sender is about to switch to ConnectionOverloaded state instantly.
//      This is bad for VOD data bursts.


namespace M {

mt_unsafe class ConnectionSenderImpl
{
  private:
    mt_const bool datagram_mode;
    IpAddress datagram_sendto_addr;

    mt_const Cb<Sender::Frontend> *frontend;
    mt_const Sender *sender;
    mt_const DeferredProcessor::Registration *deferred_reg;

    mt_const Ref<AsyncOutputStream> conn;

    // Hard queue length limit must be less or equal to soft limit.
    mt_const Count soft_msg_limit;
    mt_const Count hard_msg_limit;

    #ifdef LIBMARY_WIN32_IOCP
      struct SenderOverlapped : public Overlapped
      {
          SenderMessageEntry_PendingMessageList pending_msg_list;
          ~SenderOverlapped ();
      };

      Ref<SenderOverlapped> sender_overlapped;
      bool overlapped_pending;
    #endif

    SenderState sender_state;
    // Tracks send state as if there was no *QueueLimit states.
    bool overloaded;

    SenderMessageEntry_MessageList msg_list;
    Count num_msg_entries;

    void setSenderState (SenderState new_state);

    mt_throws AsyncIoResult sendPendingMessages_writev ();

    Result sendPendingMessages_vector_fill (Count        * mt_nonnull ret_num_iovs,
                                          #ifdef LIBMARY_WIN32_IOCP
                                            Count        * mt_nonnull ret_num_bytes,
                                            WSABUF       * mt_nonnull buffers,
                                          #else
                                            struct iovec * mt_nonnull iovs,
                                          #endif
                                            Count         num_iovs);

    void sendPendingMessages_vector_react (Count num_written);

  public:
    // Takes ownership of msg_entry.
    void queueMessage (SenderMessageEntry * mt_nonnull msg_entry);

    #ifdef LIBMARY_WIN32_IOCP
      void outputComplete ();
    #endif

    mt_throws AsyncIoResult sendPendingMessages ();

    #ifdef LIBMARY_ENABLE_MWRITEV
      void sendPendingMessages_fillIovs (Count        *ret_num_iovs,
                                         struct iovec *ret_iovs,
                                         Count         max_iovs);

      void sendPendingMessages_react (AsyncIoResult res,
                                      Size          num_written);
    #endif

    bool gotDataToSend () const { return !msg_list.isEmpty (); }

    SenderState getSenderState () const { return sender_state; }

    void setDatagramSendtoAddress (IpAddress const addr) { this->datagram_sendto_addr = addr; }

    mt_const void setConnection (AsyncOutputStream * const conn) { this->conn = conn; }

    #ifdef LIBMARY_ENABLE_MWRITEV
      Connection* getConnection () { return conn; }
    #endif

    mt_const void init (Cb<Sender::Frontend>            * const frontend,
                        Sender                          * const sender,
                        DeferredProcessor::Registration * const deferred_reg)
    {
        this->frontend = frontend;
        this->sender = sender;
        this->deferred_reg = deferred_reg;
    }

    mt_const void setLimits (Count const soft_msg_limit,
                             Count const hard_msg_limit)
    {
        this->soft_msg_limit = soft_msg_limit;
        this->hard_msg_limit = hard_msg_limit;
    }

    ConnectionSenderImpl (
                        #ifdef LIBMARY_WIN32_IOCP
                          CbDesc<Overlapped::IoCompleteCallback> const &io_complete_cb,
                        #endif
                          bool datagram_mode = false
                         );

    void release ();
};

}


#endif /* LIBMARY__CONNECTION_SENDER_IMPL__H__ */

