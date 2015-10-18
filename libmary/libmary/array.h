/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ARRAY__H__
#define LIBMARY__ARRAY__H__


#include <libmary/types.h>


namespace M {

class Array
{
  public:
    virtual void get (Size   offset,
                      Memory mem) = 0;

    virtual void set (Size        offset,
                      ConstMemory mem) = 0;

    virtual ~Array () {}
};

class ConstMemoryArray : public Array
{
  private:
    ConstMemory arr_mem;

  public:
    mt_iface (Array)
      void get (Size   const offset,
                Memory const mem)
      {
          assert (offset + mem.len() <= arr_mem.len());
          memcpy (mem.buf(), arr_mem.buf() + offset, mem.len());
      }

      void set (Size        const /* offset */,
                ConstMemory const /* mem */)
      {
          assert_hard (false);
      }
    mt_iface_end

    ConstMemoryArray (ConstMemory const arr_mem)
        : arr_mem (arr_mem)
    {}
};

}


#endif /* LIBMARY__ARRAY__H__ */

