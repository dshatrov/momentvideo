/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__DEFERRED_CONNECTION_SENDER__H__
#define LIBMARY__DEFERRED_CONNECTION_SENDER__H__


#include <libmary/intrusive_list.h>
#include <libmary/connection.h>
#include <libmary/sender.h>
#include <libmary/connection_sender_impl.h>
#include <libmary/deferred_processor.h>


namespace M {

class DeferredConnectionSenderQueue;

class DeferredConnectionSender_OutputQueue_name;
class DeferredConnectionSender_ProcessingQueue_name;

class DeferredConnectionSender : public Sender,
                                 public IntrusiveListElement<DeferredConnectionSender_OutputQueue_name>,
                                 public IntrusiveListElement<DeferredConnectionSender_ProcessingQueue_name>
{
    friend class DeferredConnectionSenderQueue;

  private:
    mt_const bool datagram_mode;

    mt_const Ref<DeferredConnectionSenderQueue> dcs_queue;

    mt_mutex (mutex)
    mt_begin
      ConnectionSenderImpl conn_sender_impl;

      bool closed;
      bool close_after_flush;
      bool ready_for_output;
      bool in_output_queue;
    mt_end

    mt_unlocks (mutex) void toGlobOutputQueue (bool add_ref,
                                               bool unlock);

    mt_unlocks (mutex) void closeIfNeeded (bool deferred_event);

    #ifdef LIBMARY_WIN32_IOCP
      static void outputIoComplete (Exception  *exc_,
                                    Overlapped *overlapped,
                                    Size        bytes_transferred,
                                    void       *cb_data);
    #else
      mt_iface (AsyncOutputStream::OutputFrontend)
        static AsyncOutputStream::OutputFrontend const conn_output_frontend;

        static void processOutput (void *_self);
      mt_iface_end
    #endif

    mt_unlocks (mutex) void doFlush (bool unlock);

  public:
    mt_iface (Sender)
      void sendMessage (SenderMessageEntry * mt_nonnull msg_entry,
                        bool                do_flush);

      void sendMessage (SenderMessageEntry  * mt_nonnull msg_entry,
                        bool                 do_flush,
                        SenderStateCallback *sender_state_cb,
                        void                *sender_state_cb_data);

      mt_mutex (mutex) void sendMessage_locked (SenderMessageEntry * mt_nonnull msg_entry,
                                                bool                do_flush);

      void flush ();

      mt_mutex (mutex) void flush_locked ();

      void closeAfterFlush ();

      void close ();

      mt_mutex (mutex) bool isClosed_locked ();

      mt_mutex (mutex) SenderState getSenderState_locked ();
    mt_iface_end

    void setDatagramSendtoAddress (IpAddress const addr)
    {
        mutex.lock ();
        conn_sender_impl.setDatagramSendtoAddress (addr);
        mutex.unlock ();
    }

    mt_const void setConnection (AsyncOutputStream * const mt_nonnull out_stream)
    {
        conn_sender_impl.setConnection (out_stream);
      #ifndef LIBMARY_WIN32_IOCP
        // TODO Figure out how to do this right in datagram mode.
        //      The problem is that a single UdpSocket is shared between many senders.
        if (!datagram_mode) {
            out_stream->setOutputFrontend (
                    CbDesc<AsyncOutputStream::OutputFrontend> (&conn_output_frontend, this, this));
        }
      #endif
    }

    mt_const void setQueue (DeferredConnectionSenderQueue * mt_nonnull dcs_queue);

    DeferredConnectionSender (EmbedContainer *embed_container,
                              bool            datagram_mode = false);

    ~DeferredConnectionSender ();
};

class DeferredConnectionSenderQueue : public Object
{
    friend class DeferredConnectionSender;

  private:
    Mutex queue_mutex;

    typedef IntrusiveList< DeferredConnectionSender, DeferredConnectionSender_OutputQueue_name >     OutputQueue;
    typedef IntrusiveList< DeferredConnectionSender, DeferredConnectionSender_ProcessingQueue_name > ProcessingQueue;

    mt_const Ref<DeferredProcessor> deferred_processor;

    DeferredProcessor::Task send_task;
    DeferredProcessor::Registration send_reg;

    mt_mutex (queue_mutex) OutputQueue output_queue;
    mt_mutex (queue_mutex) bool processing;

    mt_mutex (queue_mutex) bool released;

    static bool process (void *_self);

    #ifdef LIBMARY_ENABLE_MWRITEV
      static bool process_mwritev (void *_self);
    #endif

  public:
    mt_const void setDeferredProcessor (DeferredProcessor *deferred_processor);

    void release ();

     DeferredConnectionSenderQueue (EmbedContainer *embed_container);
    ~DeferredConnectionSenderQueue ();
};

}


#endif /* LIBMARY__DEFERRED_CONNECTION_SENDER__H__ */

