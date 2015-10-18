/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>

#include <libmary/sender.h>


namespace M {

Size
SenderState::toString_ (Memory const &mem,
                        Format const & /* fmt */) const
{
    switch (value) {
        case ConnectionReady:      return toString (mem, "ConnectionReady");
        case ConnectionOverloaded: return toString (mem, "ConnectionOverloaded");
        case QueueSoftLimit:       return toString (mem, "QueueSoftLimit");
        case QueueHardLimit:       return toString (mem, "QueueHardLimit");
    }

    unreachable ();
    return 0;
}

namespace {
    struct InformClosed_Data
    {
        Exception *exc_;
    };
}

void
Sender::informClosed (Frontend * const events,
                      void     * const cb_data,
                      void     * const _inform_data)
{
    if (events->closed) {
        InformClosed_Data * const inform_data =
                static_cast <InformClosed_Data*> (_inform_data);
        events->closed (inform_data->exc_, cb_data);
    }
}

namespace {
    struct InformSenderStateChanged_Data
    {
        SenderState sender_state;
    };
}

void
Sender::informSenderStateChanged (Frontend * const events,
                                  void     * const cb_data,
                                  void     * const _inform_data)
{
    if (events->senderStateChanged) {
        InformSenderStateChanged_Data * const inform_data =
                static_cast <InformSenderStateChanged_Data*> (_inform_data);
        events->senderStateChanged (inform_data->sender_state, cb_data);
    }
}

void
Sender::fireClosed (Exception * const exc_)
{
    InformClosed_Data inform_data = { exc_ };
    event_informer.informAll (informClosed, &inform_data);
}

mt_unlocks_locks (mutex) void
Sender::fireClosed_locked (Exception * const exc_)
{
    InformClosed_Data inform_data = { exc_ };
    mt_unlocks_locks (mutex) event_informer.informAll_locked (informClosed, &inform_data);
}

void
Sender::fireClosed_static (Exception * const exc_,
                           void      * const _self)
{
    Sender * const self = static_cast <Sender*> (_self);
    self->fireClosed (exc_);
}

void
Sender::fireClosed_deferred (DeferredProcessor::Registration * const def_reg,
                             ExceptionBuffer                 * const exc_buf)
{
    Cb <void (Exception*, void*)> cb (fireClosed_static, this, this);
    cb.call_deferred (def_reg,
                      fireClosed_static,
                      exc_buf /* extra_ref_data */,
                      exc_buf ? exc_buf->getException() : NULL);
}

void
Sender::fireSenderStateChanged (SenderState const sender_state)
{
    InformSenderStateChanged_Data inform_data = { sender_state };
    event_informer.informAll (informSenderStateChanged, &inform_data);
}

void
Sender::fireSenderStateChanged_static (SenderState   const sender_state,
                                       void        * const _self)
{
    Sender * const self = static_cast <Sender*> (_self);
    self->fireSenderStateChanged (sender_state);
}

void
Sender::fireSenderStateChanged_deferred (DeferredProcessor::Registration * const def_reg,
                                         SenderState const sender_state)
{
    Cb <void (SenderState, void*)> cb (fireSenderStateChanged_static, this, this);
    cb.call_deferred (def_reg,
                      fireSenderStateChanged_static,
                      NULL /* extra_ref_data */,
                      sender_state);
}

#if 0
void
ConnectionSenderImpl::dumpMessage (Sender::MessageEntry * const mt_nonnull msg_entry)
{
    switch (msg_entry->type) {
        case Sender::MessageEntry::Pages: {
            Sender::MessageEntry_Pages * const msg_pages = static_cast <Sender::MessageEntry_Pages*> (msg_entry);

            // Counting message length.
            Size msg_len = 0;
            {
                msg_len += msg_pages->header_len;

                PagePool::Page *cur_page = msg_pages->getFirstPage();
                while (cur_page != NULL) {
                    if (cur_page == msg_pages->getFirstPage()) {
                        assert (cur_page->data_len >= msg_pages->msg_offset);
                        msg_len += cur_page->data_len - msg_pages->msg_offset;
                    } else {
                        msg_len += cur_page->data_len;
                    }

                    cur_page = cur_page->getNextMsgPage ();
                }
            }

            // Collecting message data into a single adrray.
            Byte * const tmp_data = new Byte [msg_len];
            {
                Size pos = 0;

                memcpy (tmp_data + pos, msg_pages->getHeaderData(), msg_pages->header_len);
                pos += msg_pages->header_len;

                PagePool::Page *cur_page = msg_pages->getFirstPage();
                while (cur_page != NULL) {
                    if (cur_page == msg_pages->getFirstPage()) {
                        assert (cur_page->data_len >= msg_pages->msg_offset);
                        memcpy (tmp_data + pos,
                                cur_page->getData() + msg_pages->msg_offset,
                                cur_page->data_len - msg_pages->msg_offset);
                        pos += cur_page->data_len - msg_pages->msg_offset;
                    } else {
                        memcpy (tmp_data + pos, cur_page->getData(), cur_page->data_len);
                        pos += cur_page->data_len;
                    }

                    cur_page = cur_page->getNextMsgPage ();
                }
            }

            logLock ();
            log_locked_ (libMary_logGroup_hexdump.getLogLevel(), _func, "Message data:");
            hexdump (logs, ConstMemory (tmp_data, msg_len));
            logUnlock ();

            delete tmp_data;
        } break;
        default:
            unreachable ();
    }
}
#endif

}

