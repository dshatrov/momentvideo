/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__VFS_POSIX__H__
#define LIBMARY__VFS_POSIX__H__


#include <libmary/types.h>
#include <sys/types.h>
#include <dirent.h>

#include <libmary/vfs.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/native_file.linux.h>
#else
  #include <libmary/native_file.h>
#endif


namespace M {

class VfsPosix : public Vfs
{
  private:
    StRef<String> root_path;

    ConstMemory makePath (StRef<String> &str_holder,
                          ConstMemory    path_suffix);

    ConstMemory makePathCstr (StRef<String> &str_holder,
                              ConstMemory    path_suffix);

    class VfsPosixFile : public Vfs::VfsFile
    {
        friend class VfsPosix;

      private:
        Ref<NativeFile> native_file;

      public:
        mt_iface (Vfs::VfsFile)
          File* getFile () { return native_file; }
        mt_iface_end
    };

    class VfsPosixDirectory : public Vfs::VfsDirectory
    {
        friend class VfsPosix;

      private:
        DIR *dir;

        mt_throws Result open (ConstMemory dirname);

        VfsPosixDirectory () : dir (NULL) {}

      public:
        mt_iface (Vfs::Directory)
          mt_throws Result getNextEntry (StRef<String> &ret_name);
          mt_throws Result rewind ();
        mt_iface_end

        ~VfsPosixDirectory ();
    };

  public:
    mt_iface (Vfs)
      mt_throws Result stat (ConstMemory  name,
                             FileStat    * mt_nonnull ret_stat);

      mt_throws StRef<VfsFile> openFile (ConstMemory    _filename,
                                         Uint32         open_flags,
                                         FileAccessMode access_mode);

      mt_throws StRef<Vfs::VfsDirectory> openDirectory (ConstMemory _dirname);

      mt_throws Result createDirectory (ConstMemory _dirname);

      mt_throws Result removeFile (ConstMemory _filename);

      mt_throws Result rename (ConstMemory _old_name,
                               ConstMemory _new_name);

      mt_throws RemoveDirectoryResult removeDirectory (ConstMemory _dirname);
    mt_iface_end

    VfsPosix (ConstMemory root_path);
};

}


#endif /* LIBMARY__VFS_POSIX__H__ */

