/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ASYNC_FILE__H__
#define LIBMARY__ASYNC_FILE__H__


#include <libmary/types.h>
#include <libmary/async_input_stream.h>
#include <libmary/async_output_stream.h>

#ifdef LIBMARY_PLATFORM_WIN32
#include <libmary/connection.h>
#endif


namespace M {

class AsyncFile : public Object,
#ifdef LIBMARY_PLATFORM_WIN32
                  public Connection
#else
                  public AsyncInputStream,
                  public AsyncOutputStream
#endif
{
public:
    virtual mt_throws Result seek (FileOffset offset,
                                   SeekOrigin origin) = 0;

    virtual mt_throws Result tell (FileSize *ret_pos) = 0;

    virtual mt_throws Result sync () = 0;

    virtual mt_throws Result close (bool flush_data = true) = 0;

    AsyncFile (EmbedContainer * const embed_container) : Object (embed_container) {}
};

}


#endif /* LIBMARY__ASYNC_FILE__H__ */

