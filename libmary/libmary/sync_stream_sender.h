/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__SYNC_STREAM_SENDER__H__
#define LIBMARY__SYNC_STREAM_SENDER__H__


#include <libmary/sender.h>
#include <libmary/output_stream.h>


namespace M {

class SyncStreamSender : public Sender
{
private:
    Mutex write_mutex;

    mt_const Ref<OutputStream> output_stream;

    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) bool closed;

public:
  mt_iface (Sender)
    void sendVector (IovArray * mt_nonnull iovs,
                     bool      do_flush);

    void sendMessage (SenderMessageEntry  * mt_nonnull /* msg_entry */,
                      bool                  /* do_flush */,
                      SenderStateCallback * /* sender_state_cb */,
                      void                * /* sender_state_cb_data */)
        { unreachable(); }

    void sendMessage (SenderMessageEntry * mt_nonnull /* msg_entry */,
                      bool                 /* do_flush */)
        { unreachable(); }

    mt_mutex (mutex) void sendMessage_locked (SenderMessageEntry * mt_nonnull /* msg_entry */,
                                              bool                 /* do_flush */)
        { unreachable (); }

    void flush ();

    mt_mutex (mutex) void flush_locked ();

    void closeAfterFlush ();

    void close ();

    mt_mutex (mutex) bool isClosed_locked () {
//#error TODO return 'closed'
        return false;
    }

    mt_mutex (mutex) SenderState getSenderState_locked () { return SenderState::ConnectionReady; }
  mt_iface_end

    mt_const void init (OutputStream      * const mt_nonnull output_stream,
                        DeferredProcessor * const mt_nonnull deferred_processor)
    {
        this->output_stream = output_stream;
        deferred_reg.setDeferredProcessor (deferred_processor);
    }

    SyncStreamSender (EmbedContainer * const embed_container)
        : Sender (embed_container),
          closed (false)
    {}
};

}


#endif /* LIBMARY__SYNC_STREAM_SENDER__H__ */

