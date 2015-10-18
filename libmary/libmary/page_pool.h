/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__PAGE_POOL__H__
#define LIBMARY__PAGE_POOL__H__


#include <libmary/types.h>

#include <libmary/object.h>
#include <libmary/array.h>
#include <libmary/mutex.h>
#include <libmary/output_stream.h>
#include <libmary/stat_counter.h>
#include <libmary/timers.h>
#include <libmary/log.h>


namespace M {

extern StatCounter_AtomicInt stat_counter__page_pool_page;

// Per-thread page pools may be a good idea, but exact semantics and benefits
// are unclear atm. The main obstacle is that it's likely that a page is
// grabbed in one thread and released in another.

class PagePool : public Object
{
  private:
    Mutex mutex;

  public:
    class PageListHead;

    class Page
    {
        friend class PagePool;
        friend class PageListHead;
        friend class PageListInfo;

      private:
        // TODO this is too wasteful
        StatCounter_AtomicInt::Unit<&stat_counter__page_pool_page> stat_unit;

        #ifdef LIBMARY_MT_SAFE
          std::atomic<Size> refcnt;
        #else
          Size refcnt;
        #endif

        Page *next_pool_page;
        Page *next_msg_page;

        Page& operator = (Page const &);
        Page (Page const &);

        Page (Size const refcnt = 1)
            : refcnt (refcnt)
        {}

      public:
        Size data_len;

        Page*  getNextMsgPage () const { return next_msg_page; }
        Byte*  getData        () const { return (Byte*) this + sizeof (*this); }
        Memory mem            () const { return Memory (getData(), data_len); }

        Size getRefcount () const
        {
          #ifdef LIBMARY_MT_SAFE
            return refcnt.load (std::memory_order_relaxed);
          #else
            return refcnt;
          #endif
        }
    };

    class PageListInfo
    {
      public:
        Page *first;
        Page *last;
        Size  data_len;

        void appendList (PageListInfo * const mt_nonnull page_list)
        {
            if (page_list->first) {
                if (last)
                    last->next_msg_page = page_list->first;
                else
                    first = page_list->first;

                last = page_list->last;
            }

            data_len += page_list->data_len;
        }

        bool isEmpty () const { return data_len == 0; }

        void reset ()
        {
            first = NULL;
            last  = NULL;
            data_len = 0;
        }

        PageListInfo (Page * const first_page,
                      Page * const last_page,
                      Size   const data_len)
            : first    (first_page),
              last     (last_page),
              data_len (data_len)
        {}

        PageListInfo ()
            : first    (NULL),
              last     (NULL),
              data_len (0)
        {}
    };

    class PageListHead
    {
      public:
        Page *first;
        Page *last;

        void appendList (PageListHead * const mt_nonnull list)
        {
            if (list->first) {
                if (last)
                    last->next_msg_page = list->first;
                else
                    first = list->first;

                last = list->last;
            }
        }

        void appendPages (Page * const first)
        {
            Page *last = first;
            if (last) {
                while (last->next_msg_page)
                    last = last->next_msg_page;
            }

            PageListHead pages;
            pages.first = first;
            pages.last = last;

            appendList (&pages);
        }

        bool isEmpty () const { return first == NULL; }

        void reset ()
        {
            first = NULL;
            last = NULL;
        }

        PageListHead ()
            : first (NULL),
              last  (NULL)
        {}
    };

    mt_unsafe class PageListArray : public Array
    {
      private:
        Page * const first_page;
        Size const first_offset;
        // FIXME This field is somehow unused. Bad aliasing with doGetSet() parameter.
        Size const data_len;

        // 'cached_page' and 'cached_pos' *must* hold the last accessed
        // position in the array.
        Page *cached_page;
        Size cached_pos;
        Size last_in_page_offset;

        void doGetSet (Size offset,
                       Byte       * const data_get,
                       Byte const * const data_set,
                       Size data_len,
                       bool get);

      public:
        void get (Size   offset,
                  Memory mem);

        void set (Size        offset,
                  ConstMemory mem);

        Page* getLastAccessedPage () const { return cached_page; }

        Size getLastAccessedInPageOffset () const { return last_in_page_offset; }

        Page* getNextPageToAccess () const
        {
            if (!cached_page)
                return first_page;

            assert (last_in_page_offset <= cached_page->data_len);
            if (last_in_page_offset == cached_page->data_len)
                return cached_page->getNextMsgPage();

            return cached_page;
        }

        Size getNextInPageOffset () const
        {
            if (!cached_page)
                return first_offset;

            assert (last_in_page_offset <= cached_page->data_len);
            if (last_in_page_offset == cached_page->data_len)
                return 0;

            return last_in_page_offset;
        }

        PageListArray (Page * const first_page,
                       Size   const offset,
                       Size   const data_len)
            : first_page   (first_page),
              first_offset (offset),
              data_len     (data_len + offset),
              cached_page  (first_page),
              cached_pos   (0),
              last_in_page_offset (0)
        {}
    };

    mt_unsafe class PageListOutputStream : public OutputStream
    {
      private:
        PagePool     * const page_pool;
        PageListInfo * const page_list;

      public:
        mt_iface (OutputStream)
          mt_throws Result write (ConstMemory   const mem,
                                  Size        * const ret_nwritten)
          {
              page_pool->getFillPages (page_list, mem);

              if (ret_nwritten)
                  *ret_nwritten = mem.len();

              return Result::Success;
          }

          mt_throws Result flush ()
          {
            // No-op
              return Result::Success;
          }
        mt_iface_end

        PageListOutputStream (PagePool     * const mt_nonnull page_pool,
                              PageListInfo * const mt_nonnull page_list)
            : page_pool (page_pool),
              page_list (page_list)
        {}
    };

  private:
    mt_const Size  page_size;
    mt_const Count min_pages;
    mt_const Count delete_bulk_size;
    mt_const bool  delete_immediately;

    mt_mutex (mutex) Page *first_spare_page;
    mt_mutex (mutex) Count num_spare_pages;
    mt_mutex (mutex) Count num_busy_pages;

    mt_mutex (mutex) Page* tryGrabPage ();
    Page* grabNewPage ();

    mt_unlocks (mutex) void allocateNewPages (PageListHead * mt_nonnull page_list,
                                              Size          offset,
                                              Size          target_len);

    void allocatePages (PageListHead * mt_nonnull page_list,
                        Size          offset,
                        Size          target_len);

    template <class DataSource, bool fill>
    void doGetPages (PageListHead * mt_nonnull page_list,
                     DataSource   * mt_nonnull src,
                     Size          src_len);

    static void deleteTimerTick (void *_self);

  public:
    Size getPageSize () const { return page_size; }

    Page* grabPage ();

    void getFillPages (PageListHead * mt_nonnull page_list,
                       ConstMemory   mem);

    // TODO make this the primary method and get rid of the other overload.
    void getFillPages (PageListInfo * const mt_nonnull page_list,
                       ConstMemory    const mem)
    {
        PageListHead page_list_head;
        page_list_head.first = page_list->first;
        page_list_head.last  = page_list->last;

        getFillPages (&page_list_head, mem);

        page_list->first = page_list_head.first;
        page_list->last  = page_list_head.last;

        page_list->data_len += mem.len();
    }

    void getFillPagesFromPages (PageListHead * mt_nonnull page_list,
                                Page         * mt_nonnull from_page,
                                Size          from_offset,
                                Size          from_len);

    // TODO make this the primary method and get rid of the other overload.
    void getFillPagesFromPages (PageListInfo * const mt_nonnull page_list,
                                Page         * const mt_nonnull from_page,
                                Size           const from_offset,
                                Size           const from_len)
    {
        PageListHead page_list_head;
        page_list_head.first = page_list->first;
        page_list_head.last  = page_list->last;

        getFillPagesFromPages (&page_list_head, from_page, from_offset, from_len);

        page_list->first = page_list_head.first;
        page_list->last  = page_list_head.last;

        page_list->data_len += from_len;
    }

    void getFillPagesFromArray (PageListHead * mt_nonnull page_list,
                                Array        * mt_nonnull arr,
                                Size          offset,
                                Size          len);

    void getPages (PageListHead * mt_nonnull page_list,
                   Size len);

    void getPages (PageListInfo * mt_nonnull const page_list,
                   Size           const len)
    {
        PageListHead page_list_head;
        getPages (&page_list_head, len);
        page_list->first    = page_list_head.first;
        page_list->last     = page_list_head.last;
        page_list->data_len = len;
    }

    void pageRef (Page * const mt_nonnull page) 
    {
      #ifdef LIBMARY_MT_SAFE
        page->refcnt.fetch_add (1, std::memory_order_relaxed);
      #else
        ++page->refcnt;
      #endif
    }

    void pageUnref (Page * mt_nonnull page);

    void msgRef   (Page *first_page);
    void msgUnref (Page *first_page);

    void msgUnref (Page *first_page,
                   Size  msg_len,
                   Size  msg_offset);

    // printToPages() should never fail.
    template <class ...Args>
    void printToPages (PageListHead * const mt_nonnull page_list, Args const &...args)
    {
        // TODO switch to printToPages(PageListInfo*...)
        PageListInfo page_list_info;
        page_list_info.first = page_list->first;
        page_list_info.last  = page_list->last;

        PageListOutputStream pl_outs (this, &page_list_info);
        pl_outs.print (args...);

        page_list->first = page_list_info.first;
        page_list->last  = page_list_info.last;
    }

    // printToPages() should never fail.
    template <class ...Args>
    void printToPages (PageListInfo * const mt_nonnull page_list, Args const &...args)
    {
        PageListOutputStream pl_outs (this, page_list);
        pl_outs.print (args...);
    }

    mt_const void init (Size    page_size,
                        Count   min_pages,
                        bool    delete_immediately,
                        Timers *timers,
                        Time    delete_interval_microsec,
                        // Number of pages to delete in a single delete-pages iteration.
                        Count   delete_bulk_size);

     PagePool (EmbedContainer *embed_container);
    ~PagePool ();

    static Byte getMsgByte (Page * mt_nonnull page,
                            Size  idx)
    {
        for (;;) {
            if (page->data_len > idx)
                return page->getData() [idx];

            idx -= page->data_len;
            page = page->getNextMsgPage ();
        }

        // unreachable
        return 0;
    }

    static void dumpPages (OutputStream * mt_nonnull out,
                           PageListHead * mt_nonnull page_list,
                           Size          first_page_offs = 0);

  private:
    template <class ...Args>
    static void doLogPages (LogLevel     const loglevel,
                            Page       *page,
                            Size         const first_page_offs,
                            Args const &...args)
    {
        Page * const first_page = page;
        Count i = 1;
        while (page) {
            log_locked__ (loglevel, "Page #", i);

            if (page == first_page)
                logHex_locked__ (loglevel, page->mem().region (first_page_offs), args...);
            else
                logHex_locked__ (loglevel, page->mem(), args...);

            page = page->getNextMsgPage();
            ++i;
        }
        logs->flush ();
    }

    template <class ...Args>
    static void doLogPagesMsg (LogLevel     const loglevel,
                               Page       *page,
                               Size        msg_offs,
                               Size        msg_len,
                               Args const &...args)
    {
        if (msg_len == 0) {
            return;
        }
        assert (page);

        while (page->data_len < msg_offs) {
            page = page->getNextMsgPage ();
            assert (page);
            msg_offs -= page->data_len;
        }

        Count i = 1;
        for (;;) {
            log_locked__ (loglevel, "Page #", i);

            Size tolog = page->data_len - msg_offs;
            if (tolog > msg_len)
                tolog = msg_len;

            logHex_locked__ (loglevel, page->mem().region (msg_offs, tolog), args...);

            msg_len -= tolog;
            if (msg_len == 0)
                break;

            msg_offs = 0;

            page = page->getNextMsgPage ();
            assert (page);
            ++i;
        }
        logs->flush ();
    }

  public:
    // logLevelOn() check should be performed before calling logPages(),
    // otherwise there'll be unnecessary page_list walkthrough.
    //
    template <class ...Args>
    static void logPages (LogLevel     const loglevel,
                          Page       * const page,
                          Size         const first_page_offs,
                          Args const &...args)
    {
        exc_push_scope ();
        logLock ();
        doLogPages (loglevel, page, first_page_offs, args...);
        logUnlock ();
        exc_pop_scope ();
    }

    // logLevelOn() check should be performed before calling logPages_locked(),
    // otherwise there'll be unnecessary page_list walkthrough.
    //
    template <class ...Args>
    static void logPages_locked (LogLevel     const loglevel,
                                 Page       * const page,
                                 Size         const first_page_offs,
                                 Args const &...args)
    {
        exc_push_scope ();
        doLogPages (loglevel, page, first_page_offs, args...);
        exc_pop_scope ();
    }

    template <class ...Args>
    static void logPagesMsg (LogLevel     const loglevel,
                             Page       * const page,
                             Size         const msg_offs,
                             Size         const msg_len,
                             Args const &...args)
    {
        exc_push_scope ();
        logLock ();
        doLogPagesMsg (loglevel, page, msg_offs, msg_len, args...);
        logUnlock ();
        exc_pop_scope ();
    }

    template <class ...Args>
    static void logPagesMsg_locked (LogLevel     const loglevel,
                                    Page       * const page,
                                    Size         const msg_offs,
                                    Size         const msg_len,
                                    Args const &...args)
    {
        exc_push_scope ();
        doLogPagesMsg (loglevel, page, msg_offs, msg_len, args...);
        exc_pop_scope ();
    }

    /* Unused
     *
    static Size countPageListDataLen (Page * const first_page,
                                      Size   const msg_offset)
    {
        Size pages_data_len = 0;
        {
            PagePool::Page *cur_page = first_page;
            if (cur_page) {
                assert (msg_offset <= cur_page->data_len);
                pages_data_len += cur_page->data_len - msg_offset;
                cur_page = cur_page->getNextMsgPage ();
                while (cur_page) {
                    pages_data_len += cur_page->data_len;
                    cur_page = cur_page->getNextMsgPage ();
                }
            }
        }

        return pages_data_len;
    }
    */

    // TODO msg_len parameter
    static Count countPageListIovs (Page * const first_page,
                                    Size   const msg_offset)
    {
        if (!first_page)
            return 0;

        Count cnt = 0;

        if (first_page->data_len > msg_offset)
            ++cnt;

        Page *page = first_page->getNextMsgPage();
        while (page) {
            if (page->data_len > 0)
                ++cnt;

            page = page->getNextMsgPage();
        }

        return cnt;
    }

    static void fillPageListIovs (Page     * const first_page,
                                  Size       const msg_offset,
                                  IovArray * const iovs)
    {
        if (!first_page)
            return;

        if (first_page->data_len > msg_offset)
            iovs->addIov (first_page->getData() + msg_offset, first_page->data_len - msg_offset);

        Page *page = first_page->getNextMsgPage ();
        while (page) {
            if (page->data_len > 0)
                iovs->addIov (first_page->getData(), first_page->data_len);

            page = page->getNextMsgPage();
        }
    }

    static bool msgEqual (Page *left_page,
                          Page *right_page)
    {
        for (;;) {
            if (!left_page && !right_page)
                break;

            if (!left_page || !right_page)
                return false;

            if (left_page->data_len != right_page->data_len)
                return false;

            if (memcmp (left_page->getData(), right_page->getData(), left_page->data_len))
                return false;

            left_page  = left_page->getNextMsgPage();
            right_page = right_page->getNextMsgPage();
        }

        return true;
    }

    static bool pagesEqualToMemory (PagePool::Page *page,
                                    Size            offset,
                                    Size            len,
                                    ConstMemory      const mem)
    {
        if (len != mem.len())
            return false;

        Size pos = 0;
        while (len) {
            assert (page && page->data_len >= offset);
            Size tocmp = page->data_len - offset;
            if (tocmp > len)
                tocmp = len;

            if (memcmp (page->getData() + offset, mem.buf() + pos, tocmp))
                return false;

            len -= tocmp;
            pos += tocmp;
            offset = 0;

            page = page->getNextMsgPage();
        }

        return true;
    }

    static void copyMsgToMemory (PagePool::Page *page,
                                 Size            msg_offs,
                                 Size            msg_len,
                                 Byte           * const buf)
    {
        if (mt_unlikely (!msg_len))
            return;

        while (page && page->data_len <= msg_offs) {
            msg_offs -= page->data_len;
            page = page->getNextMsgPage();
        }

        Size pos = 0;
        for (;;) {
            Size tocopy = page->data_len - msg_offs;
            if (tocopy > msg_len)
                tocopy = msg_len;

            memcpy (buf + pos, page->getData() + msg_offs, tocopy);
            msg_len -= tocopy;
            if (msg_len == 0)
                break;

            pos += tocopy;
            msg_offs = 0;
            page = page->getNextMsgPage();
        }
    }
};

Uint32 calculateChecksumPages (PagePool::Page *first_page,
                               Size            msg_offset);

Uint64 calculateChecksumPages (PagePool::Page *first_page,
                               Size            msg_offs,
                               Size            msg_len);

}


#endif /* LIBMARY__PAGE_POOL__H__ */

