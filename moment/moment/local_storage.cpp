/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/local_storage.h>


namespace Moment {

Ref<Storage::StorageFile>
LocalStorage::openFile (ConstMemory         const filename,
                        DeferredProcessor * const mt_nonnull deferred_processor,
                        Uint32              const create_permissions)
{
    Ref<LocalStorageFile> const ls_file = grab (new (std::nothrow) LocalStorageFile (NULL /* embed_container */));

    if (!vfs->createSubdirsForFilename (filename))
        logE_ (_func, "vfs->createSubdirsForFilename(\"", filename, "\") failed: ", exc->toString());

    if (!ls_file->file->open (filename,
                              File::OpenFlags::Create | File::OpenFlags::Truncate,
                              File::AccessMode::WriteOnly,
                              create_permissions))
    {
	logE_ (_func, "file.open() failed: ", exc->toString());
	return NULL;
    }

    ls_file->conn->init (deferred_processor, ls_file->file);

    return ls_file;
}

LocalStorage::LocalStorage (EmbedContainer * const embed_container)
    : Storage (embed_container)
{
    vfs = Vfs::createDefaultLocalVfs ("/");
}

}

