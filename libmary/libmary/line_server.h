/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LINE_SERVER__H__
#define LIBMARY__LINE_SERVER__H__


#include <libmary/receiver.h>


namespace M {

class LineServer : public Object
{
public:
    struct Frontend
    {
        void (*line) (ConstMemory  line,
                      void        *cb_data);

        void (*closed) (void *cb_data);
    };

private:
    mt_const Size max_line_len;
    mt_const Cb<Frontend> frontend;

    mt_sync (processInput) Size recv_pos;

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
               Size                    max_line_len = 4096);

     LineServer (EmbedContainer *embed_container);
    ~LineServer ();
};

ConstMemory lineEscape (ConstMemory    line,
                        StRef<String> * mt_nonnull ret_ref);

}


#endif /* LIBMARY__LINE_SERVER__H__ */

