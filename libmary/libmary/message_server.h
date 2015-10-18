/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MESSAGE_SERVER__H__
#define LIBMARY__MESSAGE_SERVER__H__


#include <libmary/receiver.h>


namespace M {

class MessageServer : public Object
{
public:
    struct Frontend
    {
        void (*message) (Memory  msg,
                         void   *cb_data);

        void (*closed) (void *cb_data);
    };

private:
    mt_const Size max_msg_len;
    mt_const Cb<Frontend> frontend;

    mt_iface (Receiver::Frontend)
      static Receiver::Frontend const receiver_frontend;

      static Receiver::ProcessInputResult processInput (Memory  mem,
                                                        Size   * mt_nonnull ret_accepted,
                                                        void   *_self);

      static void processEof (Memory  unprocessed_mem,
                              void   *_self);

      static void processError (Exception *exc_,
                                Memory     unprocessed_mem,
                                void      *_self);
    mt_iface_end

public:
    void init (Receiver               *receiver,
               CbDesc<Frontend> const &frontend,
               Size                    max_msg_len);

    MessageServer (EmbedContainer *embed_container);
};

}


#endif /* LIBMARY__MESSAGE_SERVER__H__ */

