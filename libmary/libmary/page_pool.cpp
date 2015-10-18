/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <new>
#include <cstdio>

#include <libmary/util_dev.h>

#include <libmary/page_pool.h>


namespace M {

StatCounter_AtomicInt stat_counter__page_pool_page (0, "page_pool_page", 0, "libmary");

void
PagePool::PageListArray::doGetSet (Size        offset,
                                   Byte       * const data_get,
                                   Byte const * const data_set,
                                   Size         const data_len,
                                   bool         const get)
{
    if (data_len == 0)
        return;

    assert (offset + first_offset >= offset);
    offset += first_offset;
    assert (offset + data_len >= offset);

    Page *cur_page;
    Size cur_pos;
    if (cached_page &&
        offset >= cached_pos)
    {
        cur_page = cached_page;
        cur_pos = cached_pos;
    } else {
        cur_page = first_page;
        cur_pos = 0;
    }

    Size data_pos = 0;

    for (;;) {
        assert (cur_page);
        assert (cur_pos + cur_page->data_len > cur_pos);

        if (cur_pos + cur_page->data_len <= offset) {
            cur_pos += cur_page->data_len;
            cur_page = cur_page->next_msg_page;
            continue;
        }

        Size copy_from;
        if (cur_pos < offset) {
            copy_from = offset - cur_pos;
            assert (copy_from < cur_page->data_len);
        } else {
            copy_from = 0;
        }

        Size copy_to;
        if (offset + data_len < cur_pos + cur_page->data_len) {
            copy_to = offset + data_len - cur_pos;
            assert (copy_to < cur_page->data_len);
        } else {
            copy_to = cur_page->data_len;
        }

        assert (copy_to > copy_from);
        Size copy_len = copy_to - copy_from;

        if (get)
            memcpy (data_get + data_pos, cur_page->getData() + copy_from, copy_len);
        else
            memcpy (cur_page->getData() + copy_from, data_set + data_pos, copy_len);

        data_pos += copy_len;
        assert (data_pos <= data_len);
        if (data_pos == data_len) {
            // Exit point.
            cached_page = cur_page;
            cached_pos = cur_pos;
            last_in_page_offset = copy_from + copy_len;
            break;
        }

        cur_pos += cur_page->data_len;
        cur_page = cur_page->next_msg_page;
    }
}

void
PagePool::PageListArray::get (Size   const offset,
                              Memory const mem)
{
    doGetSet (offset, mem.mem() /* data_get */, NULL /* data_set */, mem.len(), true /* get */);
}

void
PagePool::PageListArray::set (Size        const offset,
                              ConstMemory const mem)
{
    doGetSet (offset, NULL /* data_get */, mem.mem() /* data_set */, mem.len(), false /* get */);
}

mt_mutex (mutex) PagePool::Page*
PagePool::tryGrabPage ()
{
    Page * const page = first_spare_page;
    if (!page)
        return NULL;

    page->next_msg_page = NULL;
    page->data_len = 0;

    first_spare_page = page->next_pool_page;

    assert (num_spare_pages > 0);
    --num_spare_pages;
    ++num_busy_pages;
    if (num_busy_pages == 1)
        ref ();

  #ifdef LIBMARY_MT_SAFE
    page->refcnt.fetch_add (1, std::memory_order_relaxed);
  #else
    ++page->refcnt;
  #endif

    return page;
}

PagePool::Page*
PagePool::grabNewPage ()
{
    // default page refcount is 1
    Byte * const buf = new (std::nothrow) Byte [sizeof (Page) + page_size];
    assert (buf);
    Page * const page = new (buf) Page;
    page->next_msg_page = NULL;
    page->data_len = 0;

    return page;
}

PagePool::Page*
PagePool::grabPage ()
{
    mutex.lock ();

    Page * const page = tryGrabPage ();
    if (!page) {
        if (num_busy_pages == 0) {
            ref ();
        }
        ++num_busy_pages;
        mutex.unlock ();

        return grabNewPage ();
    }

    mutex.unlock ();
    return page;
}

namespace {
    struct DummyDataSource {};

    class PageListDataSource
    {
        PagePool::PageListArray arr;

      public:
        void get (Byte * const mt_nonnull buf,
                  Size   const offset,
                  Size   const len)
        {
            arr.get (offset, Memory (buf, len));
        }

        PageListDataSource (PagePool::Page * const first_page,
                            Size             const offset,
                            Size             const data_len)
            : arr (first_page, offset, data_len)
        {}
    };

    class ArrayDataSource
    {
        Array * const arr;
        Size    const arr_offset;

      public:
        void get (Byte * const mt_nonnull buf,
                  Size   const offset,
                  Size   const len)
            const
        {
            arr->get (arr_offset + offset, Memory (buf, len));
        }

        ArrayDataSource (Array * const mt_nonnull arr,
                         Size    const arr_offset)
            : arr        (arr),
              arr_offset (arr_offset)
        {}
    };

    class MemoryDataSource
    {
        Byte const * const src_buf;

      public:
        void get (Byte * const mt_nonnull buf,
                  Size   const offset,
                  Size   const len)
            const
        {
            memcpy (buf, src_buf + offset, len);
        }

        MemoryDataSource (Byte const * const src_buf)
            : src_buf (src_buf)
        {}
    };

    template <class DataSource, bool fill> struct DoFill;

    template <class DataSource> struct DoFill<DataSource, false>
    {
        static void doFill (DataSource * const mt_nonnull /* src */,
                            Byte       * const /* buf */,
                            Size         const /* offset */,
                            Size         const /* len */)
        {
          // No-op
        }
    };

    template <class DataSource> struct DoFill<DataSource, true> 
    {
        static void doFill (DataSource * const mt_nonnull src,
                            Byte       * const buf,
                            Size         const offset,
                            Size         const len)
        {
            src->get (buf, offset, len);
        }
    };
} // namespace {}

mt_unlocks (mutex) void
PagePool::allocateNewPages (PageListHead * const mt_nonnull page_list,
                            Size           const offset,
                            Size           const target_len)
{
    Count const num_new_pages = (target_len - offset + (page_size - 1)) / page_size;

    if (num_new_pages && !num_busy_pages) {
        ref ();
    }
    num_busy_pages += num_new_pages;
    mutex.unlock ();

    for (Count i = 0; i < num_new_pages; ++i) {
        Page * const page = grabNewPage ();

        {
          // Dealing with the linked list.

            if (!page_list->first)
                page_list->first = page;

            if (page_list->last)
                page_list->last->next_msg_page = page;

            page_list->last = page;
        }
    }
}

void
PagePool::allocatePages (PageListHead * const mt_nonnull page_list,
                         Size          offset,
                         Size           const target_len)
{
    mutex.lock ();
    for (;;) {
        Page * const page = tryGrabPage ();
        if (!page) {
            mt_unlocks (mutex) allocateNewPages (page_list, offset, target_len);
            return;
        }

        {
          // Dealing with the linked list.

            if (!page_list->first)
                page_list->first = page;

            if (page_list->last)
                page_list->last->next_msg_page = page;

            page_list->last = page;
        }

        if (target_len - offset <= page_size)
            break;

        offset += page_size;
    }
    mutex.unlock ();
}

template <class DataSource, bool fill>
void
PagePool::doGetPages (PageListHead * const mt_nonnull page_list,
                      DataSource   * const mt_nonnull src,
                      Size           const src_len)
{
    Size offset = 0;

    Page *page = page_list->last;
    if (page) {
        if (page->data_len < page_size) {
            Size tocopy = src_len - offset;
            if (tocopy > page_size - page->data_len)
                tocopy = page_size - page->data_len;

            DoFill<DataSource, fill>::doFill (src, page->getData() + page->data_len, offset, tocopy);

            page->data_len += tocopy;
            offset += tocopy;
        }
    }

    assert (offset <= src_len);
    if (offset == src_len)
        return;

    allocatePages (page_list, offset, src_len);

    if (page)
        page = page->getNextMsgPage();
    else
        page = page_list->first;

    while (offset < src_len) {
        Size tocopy = src_len - offset;
        if (tocopy > page_size)
            tocopy = page_size;

        DoFill<DataSource, fill>::doFill (src, page->getData(), offset, tocopy);

        page->data_len = tocopy;
        offset += tocopy;

        page = page->getNextMsgPage();
    }
}

void
PagePool::getFillPages (PageListHead * const mt_nonnull page_list,
                        ConstMemory    const mem)
{
    MemoryDataSource src (mem.buf());
    doGetPages<MemoryDataSource, true /* fill */> (page_list, &src, mem.len());
}

void
PagePool::getFillPagesFromPages (PageListHead * const mt_nonnull page_list,
                                 Page         * const mt_nonnull from_page,
                                 Size           const from_offset,
                                 Size           const from_len)
{
    PageListDataSource src (from_page, from_offset, from_len);
    doGetPages<PageListDataSource, true /* fill */> (page_list, &src, from_len);
}

void
PagePool::getFillPagesFromArray (PageListHead * const mt_nonnull page_list,
                                 Array        * const mt_nonnull arr,
                                 Size           const offset,
                                 Size           const len)
{
    ArrayDataSource src (arr, offset);
    doGetPages<ArrayDataSource, true /* fill */> (page_list, &src, len);
}

void
PagePool::getPages (PageListHead * const mt_nonnull page_list,
                    Size           const len)
{
    doGetPages<DummyDataSource, false /* fill */> (page_list, /*src=*/ NULL, len);
}

void
PagePool::pageUnref (Page * const mt_nonnull page)
{
  #ifdef LIBMARY_MT_SAFE
    if (page->refcnt.fetch_sub (1, std::memory_order_release) != 1)
  #else
    if (--page->refcnt != 0)
  #endif
    {
        return;
    }

    bool do_delete = true;
    mutex.lock ();
    if (!delete_immediately || num_spare_pages < min_pages) {
        do_delete = false;

        ++num_spare_pages;
        page->next_pool_page = first_spare_page;
        first_spare_page = page;
    }

    assert (num_busy_pages > 0);
    --num_busy_pages;

    Count const tmp_nbusy = num_busy_pages;
    mutex.unlock ();
    if (tmp_nbusy == 0)
        unref ();

    if (do_delete) {
        page->~Page();
        delete[] (Byte*) page;
    }
}

void
PagePool::msgRef (Page * const first_page)
{
    Page *cur_page = first_page;
    while (cur_page) {
        Page * const next_page = cur_page->next_msg_page;
        pageRef (cur_page);
        cur_page = next_page;
    }
}

void
PagePool::msgUnref (Page * const first_page)
{
    Page *first_page_to_release = NULL;
    Page *last_page_to_release  = NULL;
    Count num_pages_to_release  = 0;
    {
        Page *page = first_page;
        while (page) {
            Page * const next_page = page->next_msg_page;

          #ifdef LIBMARY_MT_SAFE
            if (page->refcnt.fetch_sub (1, std::memory_order_release) == 1)
          #else
            if (--page->refcnt == 0)
          #endif
            {
                page->next_pool_page = first_page_to_release;
                first_page_to_release = page;
                if (!last_page_to_release) {
                    last_page_to_release = page;
                }
                ++num_pages_to_release;
            }

            page = next_page;
        }
    }

    if (!first_page_to_release)
        return;

    bool do_delete = true;
    mutex.lock ();
    if (!delete_immediately || num_spare_pages < min_pages) {
        do_delete = false;

        num_spare_pages += num_pages_to_release;
        last_page_to_release->next_pool_page = first_spare_page;
        first_spare_page = first_page_to_release;
    }

    assert (num_busy_pages >= num_pages_to_release);
    num_busy_pages -= num_pages_to_release;

    Count const tmp_nbusy = num_busy_pages;
    mutex.unlock ();
    if (tmp_nbusy == 0)
        unref ();

    if (do_delete) {
        Page *page = first_page_to_release;
        while (page) {
            Page * const next_page = page->next_pool_page;

            page->~Page();
            delete[] (Byte*) page;

            page = next_page;
        }
    }
}

void
PagePool::msgUnref (Page * const first_page,
                    Size   const msg_len,
                    Size   const msg_offset)
{
    Page *first_page_to_release = NULL;
    Page *last_page_to_release  = NULL;
    Count num_pages_to_release  = 0;
    {
        Size total_len = 0;
        Page *page = first_page;
        while (total_len < msg_len + msg_offset) {
            Page * const next_page = page->next_msg_page;
            total_len += page->data_len;

          #ifdef LIBMARY_MT_SAFE
            if (page->refcnt.fetch_sub (1, std::memory_order_release) == 1)
          #else
            if (--page->refcnt == 0)
          #endif
            {
                page->next_pool_page = first_page_to_release;
                first_page_to_release = page;
                if (!last_page_to_release) {
                    last_page_to_release = page;
                }
                ++num_pages_to_release;
            }

            page = next_page;
        }
    }

    if (!first_page_to_release)
        return;

    bool do_delete = true;
    mutex.lock ();
    if (!delete_immediately || num_spare_pages < min_pages) {
        do_delete = false;

        num_spare_pages += num_pages_to_release;
        last_page_to_release->next_pool_page = first_spare_page;
        first_spare_page = first_page_to_release;
    }

    assert (num_busy_pages >= num_pages_to_release);
    num_busy_pages -= num_pages_to_release;

    Count const tmp_nbusy = num_busy_pages;
    mutex.unlock ();
    if (tmp_nbusy == 0)
        unref ();

    if (do_delete) {
        Page *page = first_page_to_release;
        while (page) {
            Page * const next_page = page->next_pool_page;

            page->~Page();
            delete[] (Byte*) page;

            page = next_page;
        }
    }
}

void
PagePool::deleteTimerTick (void * const _self)
{
    PagePool * const self = static_cast <PagePool*> (_self);
    Count num_pages_to_release = 0;

    self->mutex.lock ();

    Page * const first_page_to_release = self->first_spare_page;
    {
        Page *first_page_to_keep = first_page_to_release;
        for (Size const num_pages_to_release_end = self->delete_bulk_size;
             num_pages_to_release < num_pages_to_release_end;
             ++num_pages_to_release)
        {
            if (   first_page_to_keep == NULL
                || num_pages_to_release >= self->delete_bulk_size)
            {
                break;
            }

            first_page_to_keep = first_page_to_keep->next_pool_page;
        }

        assert (self->num_spare_pages >= num_pages_to_release);
        self->num_spare_pages -= num_pages_to_release;

        self->first_spare_page = first_page_to_keep;
    }

    self->mutex.unlock ();

    {
        Page *page = first_page_to_release;
        for (Size i = 0; i < num_pages_to_release; ++i) {
            Page * const next_page = page->next_pool_page;

            page->~Page();
            delete[] (Byte*) page;

            page = next_page;
        }
    }
}

mt_const void
PagePool::init (Size     const page_size,
                Count    const min_pages,
                bool     const delete_immediately,
                Timers * const timers,
                Time     const delete_interval_microsec,
                Count    const delete_bulk_size)
{
    this->page_size = page_size;
    this->min_pages = min_pages;
    this->delete_bulk_size   = delete_bulk_size;
    this->delete_immediately = delete_immediately;

    Page *prv_page = NULL;
    for (Count i = 0; i < min_pages; ++i) {
        Byte * const buf = new (std::nothrow) Byte [sizeof (Page) + page_size];
        assert (buf);
        Page * const page = new (buf) Page (0 /* refcnt */);
        assert (page);

        if (i == 0)
            first_spare_page = page;

        if (prv_page)
            prv_page->next_pool_page = page;

        prv_page = page;
    }
    if (prv_page)
        prv_page->next_pool_page = NULL;

    num_spare_pages = min_pages;

    if (!delete_immediately) {
        if (timers) {
            timers->addTimer_microseconds (
                    CbDesc<Timers::TimerCallback> (deleteTimerTick, this, this),
                    delete_interval_microsec,
                    true  /* periodical */,
                    true  /* auto_delete */,
                    false /* delete_after_tick */);
        }
    }
}

PagePool::PagePool (EmbedContainer * const embed_container)
    : Object             (embed_container),
      page_size          (0),
      min_pages          (0),
      delete_bulk_size   (0),
      delete_immediately (true),
      first_spare_page   (NULL),
      num_spare_pages    (0),
      num_busy_pages     (0)
{
}

PagePool::~PagePool ()
{
    mutex.lock ();
    assert (num_busy_pages == 0);

    Page *page = first_spare_page;
    while (page) {
        Page * const next_page = page->next_pool_page;

        page->~Page();
        delete[] (Byte*) page;

        page = next_page;
    }

    mutex.unlock ();
}

void
PagePool::dumpPages (OutputStream * const mt_nonnull out,
                     PageListHead * const mt_nonnull page_list,
                     Size           const first_page_offs)
{
    Page *page = page_list->first;
    Count i = 1;
    while (page) {
        out->print ("\nPage #", i, "\n");

        if (page == page_list->first)
            hexdump (out, page->mem().region (first_page_offs));
        else
            hexdump (out, page->mem());

        page = page->getNextMsgPage();
        ++i;
    }
    out->print ("\n");
    out->flush ();
}

}

