/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__SENDER__H__
#define LIBMARY__SENDER__H__


#include <libmary/types.h>
#include <new>

#include <libmary/intrusive_list.h>
#include <libmary/cb.h>
#include <libmary/informer.h>
#include <libmary/exception.h>
#include <libmary/page_pool.h>
#include <libmary/sender_message_entry.h>
#include <libmary/log.h>


namespace M {

struct SenderState
{
    enum Value {
                              // The comments below describe usual behavior of
                              // user's code when processing sendStateChanged()
                              // notifications.

        ConnectionReady,      // Fast client, no flow control restrictions.

        ConnectionOverloaded, // Slow client, dropping disposable messages.

        QueueSoftLimit,       // Send queue is full, blocking input from
                              // the client as an extra countermeasure.

        QueueHardLimit        // Send queue growth is out of control.
                              // Disconnecting the client.
    };
    operator Value () const { return value; }
    SenderState (Value const value) : value (value) {}
    SenderState () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;
};

class Sender : public Object
{
  protected:
    StateMutex mutex;

  public:
    typedef mt_mutex (mutex) void SenderStateCallback (SenderState  sender_state,
                                                       void        *cb_data);

    struct Frontend {
        // This callback is called with Sender::mutex held, which means that
        // it is forbidden to call any methods of Sender from the callback.
        void (*senderStateChanged) (SenderState  sender_state,
                                    void        *cb_data);

//#warning TODO pass "close_input" parameter to communicate user's' intentions to the server (HttpServer mostly).
        void (*closed) (Exception *exc_,
                        void      *cb_data);
    };

  protected:
    mt_const Cb<Frontend> frontend;

    Informer_<Frontend> event_informer;

    static void informClosed (Frontend *events,
                              void     *cb_data,
                              void     *inform_data);

    static void informSenderStateChanged (Frontend *events,
                                          void     *cb_data,
                                          void     *inform_data);

    void fireClosed (Exception *exc_);

    mt_unlocks_locks (mutex) void fireClosed_locked (Exception *exc_);

    static void fireClosed_static (Exception *exc_,
                                   void      *_self);

    void fireClosed_deferred (DeferredProcessor::Registration *def_reg,
                              ExceptionBuffer                 *exc_buf);

    void fireSenderStateChanged (SenderState sender_state);

    static void fireSenderStateChanged_static (SenderState  sender_state,
                                               void        *_self);

  public:
    Informer_<Frontend>* getEventInformer ()
        { return &event_informer; }

    // public for ConnectionSenderImpl.
    void fireSenderStateChanged_deferred (DeferredProcessor::Registration *def_reg,
                                          SenderState sender_state);

    virtual void sendVector (IovArray * mt_nonnull /* iovs */,
                             bool       /* do_flush */)
        { unreachable (); }

    // Takes ownership of msg_entry.
    virtual void sendMessage (SenderMessageEntry * mt_nonnull msg_entry,
                              bool                do_flush) = 0;

    // Takes ownership of msg_entry.
    virtual void sendMessage (SenderMessageEntry  * mt_nonnull msg_entry,
                              bool                 do_flush,
                              SenderStateCallback *sender_state_cb,
                              void                *sender_state_cb_data) = 0;

    virtual mt_mutex (mutex) void sendMessage_locked (SenderMessageEntry * mt_nonnull msg_entry,
                                                      bool                do_flush) = 0;

    virtual void flush () = 0;

    virtual mt_mutex (mutex) void flush_locked () = 0;

    // Frontend::closed() will be called after message queue becomes empty.
    virtual void closeAfterFlush () = 0;

    // Frontend::closed() will be called (deferred callback invocation).
    virtual void close () = 0;

    virtual mt_mutex (mutex) bool isClosed_locked () = 0;

    virtual mt_mutex (mutex) SenderState getSenderState_locked () = 0;

    void lock   () { mutex.lock   (); }
    void unlock () { mutex.unlock (); }

    void sendMessage_fromVector (IovArray * const mt_nonnull iovs,
                                 bool       const do_flush,
                                 PagePool * const mt_nonnull page_pool)
    {
        PagePool::PageListInfo page_list;
        for (Count i = 0; i < iovs->i; ++i) {
            ConstMemory const mem = iovs->getIov (i);
            page_pool->getFillPages (&page_list, mem);
        }

        SenderMessageEntry_Pages * const msg = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
        msg->init (page_list.first, page_pool, /*msg_offs=*/ 0, page_list.data_len);

        sendMessage (msg, do_flush);
    }

    void sendPages (PagePool       * const mt_nonnull page_pool,
                    PagePool::Page * const mt_nonnull first_page,
                    Size             const msg_offset,
                    Size             const msg_len,
                    bool             const do_flush)
    {
        SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (0 /* max_header_len */);
        msg_pages->init (first_page, page_pool, msg_offset, msg_len);

        sendMessage (msg_pages, do_flush);
    }

    template <class ...Args>
    void send (PagePool   * const mt_nonnull page_pool,
               bool         const do_flush,
               Args const &...args)
    {
        PagePool::PageListInfo page_list;
        // TODO If data length fits into vslab msg header, then use it instead of pages.
        //      ^ This can be achieved by constructing SenderMessageEntry_Pages manually.
        page_pool->printToPages (&page_list, args...);

        SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
        msg_pages->init (page_list.first, page_pool, /*msg_offs=*/ 0, page_list.data_len);

        sendMessage (msg_pages, do_flush);
    }

    void seekSet (Uint64 const offset)
    {
        SenderMessageEntry_Seek * const msg = new (std::nothrow) SenderMessageEntry_Seek (offset);
        assert (msg);

        sendMessage (msg, false /* do_flush */);
    }

    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }

    Sender (EmbedContainer * const embed_container)
        : Object         (embed_container),
          event_informer (this /* outer_object */, &mutex)
    {}
};

}


#endif /* LIBMARY__SENDER__H__ */

