/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ARRAY_OUTPUT_STREAM__H__
#define LIBMARY__ARRAY_OUTPUT_STREAM__H__


#include <libmary/output_stream.h>
#include <libmary/array.h>


namespace M {

// Note: This class is unused.
//
mt_unsafe class ArrayOutputStream : public OutputStream
{
  private:
    Array * const arr;
    Size offset;

  public:
    mt_iface (OutputStream)
      mt_throws Result write (ConstMemory   const mem,
                              Size        * const ret_nwritten)
      {
          arr->set (offset, mem);
          offset += mem.len();

          if (ret_nwritten)
              *ret_nwritten = mem.len();

          return Result::Success;
      }

      mt_throws Result flush ()
      {
        // No-op
          return Result::Success;
      }
    mt_iface_end

    ArrayOutputStream (Array * const mt_nonnull arr)
        : arr    (arr),
          offset (0)
    {}
};

}


#endif /* LIBMARY__ARRAY_OUTPUT_STREAM__H__ */

