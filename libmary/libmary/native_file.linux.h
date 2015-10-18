/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__NATIVE_FILE_LINUX__H__
#define LIBMARY__NATIVE_FILE_LINUX__H__


#include <libmary/file.h>


namespace M {

class NativeFile : public File,
                   public Object
{
  private:
    mt_const int fd;

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

    mt_throws Result stat (FileStat * mt_nonnull ret_stat);

    mt_throws Result getModificationTime (struct tm * mt_nonnull ret_tm);

    mt_const void setFd (int fd);

  #ifdef LIBMARY_ENABLE_MWRITEV
    int getFd () { return fd; }
  #endif

    mt_const mt_throws Result open (ConstMemory filename,
                                    Uint32      open_flags,
                                    AccessMode  access_mode,
                                    Uint32      create_permissions = LIBMARY__DEFAULT_CREATE_PERMISSIONS);

     NativeFile (EmbedContainer *embed_container,
                 int fd);

     NativeFile (EmbedContainer *embed_container);
    ~NativeFile ();
};

}


#endif /* LIBMARY__NATIVE_FILE_LINUX__H__ */

