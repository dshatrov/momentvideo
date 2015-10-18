/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>

#include <libmary/cached_file.h>


namespace M {

mt_throws Result
CachedFile::syncFilePos (FileSize const desired_pos)
{
    if (file_pos != desired_pos) {
        if (mt_unlikely (!file->seekBeg (desired_pos)))
            return Result::Failure;

        file_pos = desired_pos;
    }

    return Result::Success;
}

CachedFile::PageEntry*
CachedFile::addPage (FileSize const page_pos)
{
    if (num_pages >= max_pages) {
        assert (!lru_list.isEmpty());
        PageEntry * const entry = lru_list.getFirst();

        lru_list.remove (entry);
        page_tree.remove (entry);

        entry->offs = page_pos;
        entry->page->data_len = 0;
        entry->eof = false;

        lru_list.append (entry);
        page_tree.add (entry);

        return entry;
    }

    PageEntry * const entry = new (std::nothrow) PageEntry;
    assert (entry);

    entry->offs = page_pos;
    entry->page = page_pool->grabPage ();
    entry->page->data_len = 0;
    entry->eof = false;

    lru_list.append (entry);
    page_tree.add (entry);

    ++num_pages;
    return entry;
}

mt_throws IoResult
CachedFile::read (Memory   const mem,
                  Size   * const ret_nread)
{
    if (mt_likely (ret_nread))
        *ret_nread = 0;

    if (mt_unlikely (closed)) {
        exc_throw (InternalException, InternalException::IncorrectUsage);
        return IoResult::Error;
    }

    if (mt_unlikely (mem.len() == 0))
        return IoResult::Normal;

    Size const page_size = page_pool->getPageSize();
    Size offs = (Size) (pos % page_size);

    FileSize page_pos = pos - offs;
    Size total_nread = 0;
    for (;;) {
        PageEntry *entry = page_tree.lookup (page_pos);
        if (!entry) {
            entry = addPage (page_pos);

            if (mt_unlikely (!syncFilePos (page_pos)))
                return IoResult::Error;

            Size nread = 0;
            IoResult const res = file->readFull (Memory (entry->page->getData(), page_size), &nread);
            if (mt_unlikely (res == IoResult::Error))
                return IoResult::Error;

            if (res == IoResult::Eof) {
                entry->eof = true;
            } else {
                assert (res == IoResult::Normal);

                if (nread < page_size)
                    entry->eof = true;
                else
                if (mt_unlikely (nread != page_size)) {
                    logE_ (_this_func, "unexpected nread: ", nread);
                    exc_throw (InternalException, InternalException::BackendMalfunction);
                    return IoResult::Error;
                }

                file_pos = page_pos + nread;

                entry->page->data_len = nread;
            }
        }

        if (entry->page->data_len <= offs) {
            if (mt_likely (entry->eof)) {
                if (total_nread == 0)
                    return IoResult::Eof;

                return IoResult::Normal;
            }

            logE_ (_this_func, "unexpected partial data");
            exc_throw (InternalException, InternalException::BackendMalfunction);
            return IoResult::Error;
        }

        Size tocopy = entry->page->data_len - offs;
        assert (mem.len() > total_nread);
        if (tocopy > mem.len() - total_nread)
            tocopy = mem.len() - total_nread;

        memcpy (mem.buf() + total_nread, entry->page->getData() + offs, tocopy);
        pos += tocopy;
        total_nread += tocopy;
        if (mt_likely (ret_nread))
            *ret_nread = total_nread;

        if (total_nread == mem.len()) {
            break;
        }
        assert (total_nread < mem.len());

        if (entry->eof)
            break;

        page_pos += page_size;
        offs = 0;
    }

    return IoResult::Normal;
}

mt_throws Result
CachedFile::close (bool const flush_data)
{
    closed = true;

    {
        LruList::iterator iter (lru_list);
        while (!iter.done()) {
            PageEntry * const entry = iter.next ();
            page_pool->pageUnref (entry->page);
            delete entry;
        }
        lru_list.clear ();
        page_tree.clear ();
    }

    return file->close (flush_data);
}

}

