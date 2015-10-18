/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__
#define LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__


#include <libmary/async_output_stream.h>
#include <libmary/sender.h>
#include <libmary/connection_sender_impl.h>


namespace M {

class ImmediateConnectionSender : public Sender
{
  private:
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) ConnectionSenderImpl conn_sender_impl;

    mt_mutex (mutex) bool closed;
    mt_mutex (mutex) bool close_after_flush;
    mt_mutex (mutex) bool ready_for_output;

    mt_unlocks (mutex) void closeIfNeeded (bool deferred_event,
                                           bool unlock);

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

    mt_mutex (mutex) mt_unlocks (mutex) void doFlush (bool unlock);

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

    mt_const void setConnection (AsyncOutputStream * const mt_nonnull out_stream)
    {
        conn_sender_impl.setConnection (out_stream);
      #ifndef LIBMARY_WIN32_IOCP
        out_stream->setOutputFrontend (
                CbDesc<AsyncOutputStream::OutputFrontend> (&conn_output_frontend, this, this));
      #endif
    }

    mt_const void init (DeferredProcessor * const mt_nonnull deferred_processor)
    {
        deferred_reg.setDeferredProcessor (deferred_processor);
    }

     ImmediateConnectionSender (EmbedContainer *embed_container);
    ~ImmediateConnectionSender ();
};

}


#endif /* LIBMARY__IMMEDIATE_CONNECTION_SENDER__H__ */

