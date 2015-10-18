/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__CACHED_FILE__H__
#define LIBMARY__CACHED_FILE__H__


#include <libmary/file.h>
#include <libmary/page_pool.h>
#include <libmary/intrusive_avl_tree.h>


namespace M {

// Reading and writing is not in sync: read() may return old data
// after write() is done.
//
mt_unsafe class CachedFile : public File
{
  private:
    struct PageEntry : public IntrusiveAvlTree_Node<>,
                       public IntrusiveListElement<>
    {
        mt_const PagePool::Page *page;
        FileSize offs;
        bool eof;
    };

    mt_const Ref<File>      file;
    mt_const Ref<PagePool>  page_pool;
    mt_const Count          max_pages;

    Count num_pages;

    FileSize file_pos;
    FileSize pos;

    typedef IntrusiveAvlTree< PageEntry,
                              MemberExtractor< PageEntry,
                                               FileSize,
                                               &PageEntry::offs >,
                              DirectComparator<FileSize> >
            PageTree;

    PageTree page_tree;

    typedef IntrusiveList<PageEntry> LruList;

    LruList lru_list;

    bool closed;

    mt_throws Result syncFilePos (FileSize desired_pos);

    PageEntry* addPage (FileSize page_pos);

  public:
    mt_iface (File)
      mt_iface (InputStream)
        mt_throws IoResult read (Memory  mem,
                                 Size   *ret_nread);
      mt_iface_end

      mt_iface (OutputStream)
        mt_throws Result write (ConstMemory   const mem,
                                Size        * const ret_nwritten)
        {
            if (mt_likely (ret_nwritten))
                *ret_nwritten = 0;

            if (mt_unlikely (!syncFilePos (pos)))
                return Result::Failure;

            Size nwritten = 0;
            if (mt_unlikely (!file->write (mem, &nwritten)))
                return Result::Failure;

            pos += nwritten;
            file_pos = pos;

            if (mt_likely (ret_nwritten))
                *ret_nwritten = nwritten;

            return Result::Success;
        }

        mt_throws Result flush ()
            { return file->flush (); }
      mt_iface_end

      mt_throws Result seek (FileOffset const offset,
                             SeekOrigin const origin)
      {
          switch ((SeekOrigin::Value) origin) {
              case SeekOrigin::Cur: {
                  if (offset >= 0)
                      pos += (FileSize) offset;
                  else
                      pos -= (FileSize) -offset;

              } break;
              case SeekOrigin::Beg: {
                  pos = (FileSize) offset;
              } break;
              case SeekOrigin::End: {
                  exc_throw (InternalException, InternalException::IncorrectUsage);
                  return Result::Failure;
              } break;
          }

          return Result::Success;
      }

      mt_throws Result tell (FileSize *ret_pos)
      {
          if (mt_likely (ret_pos))
              *ret_pos = pos;

          return Result::Success;
      }

      mt_throws Result sync () { return file->sync (); }

      mt_throws Result close (bool flush_data = true);
    mt_iface_end

    mt_const void init (File     * const mt_nonnull file,
                        PagePool * const mt_nonnull page_pool,
                        Count      const max_pages)
    {
        assert (max_pages > 0);

        this->file      = file;
        this->page_pool = page_pool;
        this->max_pages = max_pages;
    }

    CachedFile ()
        : max_pages (0),
          num_pages (0),
          file_pos  (0),
          pos       (0),
          closed    (false)
    {}

    ~CachedFile ()
    {
        if (!closed)
            close (true /* do_flush */);
    }
};

}


#endif /* LIBMARY__CACHED_FILE__H__ */

