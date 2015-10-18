/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>
#include <libmary/util_dev.h>

#include <libmary/message_server.h>


namespace M {

Receiver::Frontend const MessageServer::receiver_frontend = {
    processInput,
    processEof,
    processError,
    NULL /* processInputFrom */
};

Receiver::ProcessInputResult
MessageServer::processInput (Memory   const _mem,
                             Size   * const mt_nonnull ret_accepted,
                             void   * const  _self)
{
    MessageServer * const self = static_cast <MessageServer*> (_self);
    Memory mem = _mem;

    *ret_accepted = 0;

    for (;;) {
        if (mem.len() < 4)
            return Receiver::ProcessInputResult::Again;

        Size const msg_len = ((Uint32) mem.buf() [0] << 24) |
                             ((Uint32) mem.buf() [1] << 16) |
                             ((Uint32) mem.buf() [2] <<  8) |
                             ((Uint32) mem.buf() [3] <<  0);

        if (msg_len > self->max_msg_len)
            return Receiver::ProcessInputResult::Error;

        if (msg_len + 4 > mem.len())
            return Receiver::ProcessInputResult::Again;

        if (self->frontend)
            self->frontend.call (self->frontend->message, mem.region (4, 4 + msg_len));

        mem = mem.region (4 + msg_len);
        *ret_accepted += (4 + msg_len);
    }

    return Receiver::ProcessInputResult::Normal;
}

void
MessageServer::processEof (Memory   const /* unprocessed_mem */,
                           void   * const _self)
{
    MessageServer * const self = static_cast <MessageServer*> (_self);
    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

void
MessageServer::processError (Exception * const /* exc_ */,
                             Memory      const /* unprocessed_mem */,
                             void      * const _self)
{
    MessageServer * const self = static_cast <MessageServer*> (_self);
    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

void
MessageServer::init (Receiver         * const  receiver,
                     CbDesc<Frontend>   const &frontend,
                     Size               const  max_msg_len)
{
    this->frontend = frontend;
    this->max_msg_len = max_msg_len;

    receiver->setFrontend (CbDesc<Receiver::Frontend> (&receiver_frontend, this, this));
}

MessageServer::MessageServer (EmbedContainer * const embed_container)
    : Object (embed_container),
      max_msg_len (0)
{
}

}

