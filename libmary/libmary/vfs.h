/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__VFS__H__
#define LIBMARY__VFS__H__


#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/native_file.linux.h>
#else
  #include <libmary/native_file.h>
#endif

namespace M {

class Vfs : public StReferenced
{
  public:
    typedef NativeFile::FileType FileType;
    typedef NativeFile::FileStat FileStat;

    class VfsFile : public StReferenced
    {
      public:
        virtual File* getFile () = 0;
    };

    class VfsDirectory : public StReferenced
    {
      public:
	virtual mt_throws Result getNextEntry (StRef<String> &ret_name) = 0;
	virtual mt_throws Result rewind () = 0;
    };

    virtual mt_throws Result stat (ConstMemory  name,
                                   FileStat    * mt_nonnull ret_stat) = 0;

    virtual mt_throws StRef<VfsFile> openFile (ConstMemory    filename,
                                               Uint32         open_flags,
                                               FileAccessMode access_mode) = 0;

    virtual mt_throws StRef<VfsDirectory> openDirectory (ConstMemory dirname) = 0;

    virtual mt_throws Result createDirectory (ConstMemory dirname) = 0;

    virtual mt_throws Result removeFile (ConstMemory filename) = 0;

    virtual mt_throws Result rename (ConstMemory old_name,
                                     ConstMemory new_name) = 0;

    enum_beg (RemoveDirectoryResult)
        enum_values (Success, Failure, NotEmpty)
    enum_end (RemoveDirectoryResult)

    virtual mt_throws RemoveDirectoryResult removeDirectory (ConstMemory dirname) = 0;

    mt_throws Result createSubdirs (ConstMemory dirs_path);

    mt_throws Result createSubdirsForFilename (ConstMemory const filename)
    {
        if (Byte const * const file_part = (Byte const*) memrchr (filename.mem(), '/', filename.len()))
            return createSubdirs (filename.region (0, file_part - filename.mem()));

        return Result::Success;
    }

    mt_throws Result removeSubdirs (ConstMemory dirs_path);

    mt_throws Result removeSubdirsForFilename (ConstMemory const filename)
    {
        if (Byte const * const file_part = (Byte const*) memrchr (filename.mem(), '/', filename.len()))
            return removeSubdirs (filename.region (0, file_part - filename.mem()));

        return Result::Success;
    }

    mt_throws Result removeDirectoryWithAllContents (ConstMemory dirname);

    static mt_throws StRef<Vfs> createDefaultLocalVfs (ConstMemory root_path);

    virtual ~Vfs () {}
};

}


#endif /* LIBMARY__VFS__H__ */

