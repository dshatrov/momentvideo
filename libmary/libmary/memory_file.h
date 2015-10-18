/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MEMORY_FILE__H__
#define LIBMARY__MEMORY_FILE__H__


#include <libmary/file.h>


namespace M {

mt_unsafe class MemoryFile : public File
{
  protected:
    Memory array_mem;
    Size pos;

  public:
    mt_iface (File)
      mt_iface (InputStream)
        mt_throws IoResult read (Memory  mem,
                                 Size   *ret_nread);
      mt_iface_end

      mt_iface (OutputStream)
        mt_throws Result write (ConstMemory  mem,
                                Size        *ret_nwritten);

        mt_throws Result flush ();
      mt_iface_end

      mt_throws Result seek (FileOffset offset,
                             SeekOrigin origin);

      mt_throws Result tell (FileSize *ret_pos);

      mt_throws Result sync ();

      mt_throws Result close (bool flush_data = true);
    mt_iface_end

    MemoryFile (Memory mem);
};

}


#endif /* LIBMARY__MEMORY_FILE__H__ */

