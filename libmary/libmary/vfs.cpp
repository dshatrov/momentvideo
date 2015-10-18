/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/util_base.h>
#include <libmary/vfs_posix.h>
#include <libmary/log.h>

#include <libmary/vfs.h>


namespace M {

mt_throws Result
Vfs::createSubdirs (ConstMemory const dirs_path)
{
    ConstMemory tail = dirs_path;
    for (;;) {
        ConstMemory cur_dir = dirs_path;
        Byte const * const slash = (Byte const *) memchr (tail.mem(), '/', tail.len());
        if (slash) {
            tail = tail.region (slash - tail.mem() + 1);
            cur_dir = dirs_path.region (0, slash - dirs_path.mem());
        }

        if (cur_dir.len() > 0) {
            if (!createDirectory (cur_dir))
                return Result::Failure;
        }

        if (!slash)
            break;
    }

    return Result::Success;
}

mt_throws Result
Vfs::removeSubdirs (ConstMemory const dirs_path)
{
    ConstMemory head = dirs_path;
    for (;;) {
        if (head.len() == 0)
            break;

        {
            RemoveDirectoryResult const res = mt_throws removeDirectory (head);
            if (res == RemoveDirectoryResult::NotEmpty) {
                return Result::Success;
            } else
            if (res == RemoveDirectoryResult::Failure) {
                logE_ (_func, "Could not remove directory \"", head, "\": ", exc->toString());
                return Result::Failure;
            }
            assert (res == RemoveDirectoryResult::Success);
        }

        Byte const * const slash = (Byte const *) memrchr (head.mem(), '/', head.len());
        if (!slash)
            break;

        head = head.region (0, slash - head.mem());
    }

    return Result::Success;
}

mt_throws Result
Vfs::removeDirectoryWithAllContents (ConstMemory const dirname)
{
    StRef<VfsDirectory> const dir = mt_throws openDirectory (dirname);
    if (!dir) {
        // TODO Distinguish between "could not delete" and "dir does not exist".
//        exc_push (InternalException, InternalException::BackendError);
//        return Result::Failure;
        return Result::Success;
    }

    for (;;) {
        StRef<String> entry;
        if (! mt_throws dir->getNextEntry (entry)) {
            exc_push (InternalException, InternalException::BackendError);
            return Result::Failure;
        }

        if (!entry)
            break;

        if (equal (entry, ".") || equal (entry, ".."))
            continue;

        StRef<String> const path = makeString (dirname, (dirname.len() ? "/" : ""), entry);
        FileStat file_stat;
        if (! mt_throws stat (path, &file_stat))
            logW_ (_this_func, "stat(\"", path, "\") failed: ", exc->toString());

        if (file_stat.file_type == FileType::Directory) {
            if (! mt_throws removeDirectoryWithAllContents (path))
                logW_ (_this_func, "removeDirectoryWithAllContents() failed: ", exc->toString());
        } else {
            if (! mt_throws removeFile (path))
                logW_ (_this_func, "removeFile (\"", path, "\") failed: ", exc->toString());
        }
    }

    {
        RemoveDirectoryResult const res = mt_throws removeDirectory (dirname);
        if (res == RemoveDirectoryResult::NotEmpty) {
            exc_throw (InternalException, InternalException::BackendError);
            return Result::Failure;
        } else
        if (res == RemoveDirectoryResult::Failure) {
            return Result::Failure;
        }
        assert (res == RemoveDirectoryResult::Success);
    }

    return Result::Success;
}

mt_throws StRef<Vfs>
Vfs::createDefaultLocalVfs (ConstMemory const root_path)
{
    // VfsPosix appears to work under mingw as well.
    return st_grab (static_cast <Vfs*> (new (std::nothrow) VfsPosix (root_path)));
}

}

