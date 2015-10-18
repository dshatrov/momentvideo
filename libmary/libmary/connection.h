/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__CONNECTION__H__
#define LIBMARY__CONNECTION__H__


#include <libmary/types.h>
#include <libmary/async_input_stream.h>
#include <libmary/async_output_stream.h>


namespace M {

class Connection : public Object,
                   public AsyncInputStream,
		   public AsyncOutputStream
{
public:
    // Note: shutdown() might be useful to allow closing before dtor is called.

  #ifdef LIBMARY_ENABLE_MWRITEV
    virtual int getFd () = 0;
  #endif

    Connection (EmbedContainer * const embed_container) : Object (embed_container) {}
};

}


#endif /* LIBMARY__CONNECTION__H__ */

