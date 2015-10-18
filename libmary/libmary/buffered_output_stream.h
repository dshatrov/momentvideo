/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__BUFFERED_OUTPUT_STREAM__H__
#define LIBMARY__BUFFERED_OUTPUT_STREAM__H__


#include <libmary/output_stream.h>


namespace M {

mt_unsafe class BufferedOutputStream : public OutputStream
{
  private:
    Ref<OutputStream> const out;

    Byte * const data_buf;
    Size   const data_len;

    Size in_buffer;

    mt_throws Result flushBuffer ();

  public:
    mt_iface (OutputStream)
      mt_throws Result write (ConstMemory  mem,
                              Size        *ret_nwritten);

      mt_throws Result flush ();
    mt_iface_end

     BufferedOutputStream (OutputStream * const mt_nonnull out,
                           Size buf_len);

    ~BufferedOutputStream ();
};

}


#endif /* LIBMARY__BUFFERED_OUTPUT_STREAM__H__ */

