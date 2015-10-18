#ifndef LIBMARY__STREAMING_PAGE_POOL__H__
#define LIBMARY__STREAMING_PAGE_POOL__H__


#include <libmary/array.h>
#include <libmary/timers.h>


namespace M {

class StreamingMessage;
class StreamingPage;

class StreamingPagePool : public Object
{
    friend class StreamingMessage;
    friend class StreamingPage;

  private:
    Mutex mutex;

    mt_const Size  page_size;
    mt_const Count min_pages;
    mt_const Count delete_bulk_size;
    mt_const bool  delete_immediately;

    mt_mutex (mutex) StreamingPage *first_spare_page;
    mt_mutex (mutex) Count num_spare_pages;

    static mt_const StreamingPagePool *page_pool_instance;

    void doPageUnref (StreamingPage * mt_nonnull page);

    void doMsgUnref (StreamingMessage const *msg);

    static void deleteTimerTick (void *_self);

    mt_const void init (Size    page_size,
                        Count   min_pages,
                        bool    delete_immediately,
                        Timers *timers,
                        Time    delete_interval_microsec,
                        Count   delete_bulk_size);

  public:
    Size getPageSize () const { return page_size; }

    StreamingPage* getPages (Size            len,
                             StreamingPage **ret_last_page = NULL);

    static StreamingPagePool* getInstance () { return page_pool_instance; }

    void createInstance (Size    page_size,
                         Count   min_pages,
                         bool    delete_immediately,
                         Timers *timers,
                         Time    delete_interval_microsec,
                         Count   delete_bulk_size);

     StreamingPagePool (EmbedContainer *embed_container);
    ~StreamingPagePool ();
};

struct StreamingMessage
{
    StreamingPage *first_page;
    Size           msg_offs;
    Size           msg_len;

    void msgRef   ();
    void msgUnref () { StreamingPagePool::getInstance()->doMsgUnref (this); }

    StreamingMessage (StreamingPage * const first_page,
                      Size            const msg_offs,
                      Size            const msg_len)
        : first_page (first_page),
          msg_offs   (msg_offs),
          msg_len    (msg_len)
    {}
};

class StreamingPage
{
    friend class StreamingGetPagesContext;
    friend class StreamingFillPagesContext;
    friend class StreamingPagePool;
    friend class StreamingMessage;
    friend class StreamingMessageReader;

  private:
    // Note: 'next_page' is only valid for pages which are part of a message
    // spanning across page boundary. Accessing 'next_page' of the last page
    // of a message would result in a race (think undefined behavior) unless
    // the message ends precisely on the last page's boundary.
    // That's why the use of StreamingMessageReader is mandatory: it prevents
    // from accidentally introducing this data race.
    StreamingPage *next_page;

    Atomic<Size> refcnt;

    StreamingPage (Size const refcnt) : refcnt (refcnt) {}

  public:
    void pageRef   () { refcnt.fetch_add (1, std::memory_order_relaxed); }
    void pageUnref () { StreamingPagePool::getInstance()->doPageUnref (this); }

    Byte* getData () const { return (Byte*) this + sizeof (*this); }

    Size getRefcount () const { return refcnt.load (std::memory_order_relaxed); }
};

mt_unsafe class StreamingGetPagesContext
{
  private:
    StreamingPage *cur_page;
    Size cur_offs;

  public:
    StreamingPage* getPages (Size  len,
                             Size * mt_nonnull ret_offs);

    StreamingGetPagesContext ()
        : cur_page (NULL),
          cur_offs (0)
    {}

    ~StreamingGetPagesContext ()
    {
        if (cur_page)
            cur_page->pageUnref ();
    }
};

mt_unsafe class StreamingFillPagesContext
{
  private:
    StreamingPage *cur_page;
    Size           cur_offs;
    Size           cur_len;

    Size const total_len;

  public:
    void fillPages (ConstMemory mem);

    void fillPagesFromPages (StreamingMessage msg);

    void fillPagesFromArray (Array * mt_nonnull arr,
                             Size   offset,
                             Size   len);

    void doneAssert () const { assert_hard (cur_len == total_len); }

    StreamingFillPagesContext (StreamingPage * const first_page,
                               Size            const msg_offs,
                               Size            const msg_len)
        : cur_page  (first_page),
          cur_offs  (msg_offs),
          cur_len   (msg_len),
          total_len (0)
    {}
};

mt_unsafe class StreamingMessageReader
{
  private:
    StreamingPage *cur_page;
    Size msg_offs;
    Size msg_len;

    Size const page_size;

  public:
    Memory getNextDataBlock ()
    {
        if (!msg_len)
            return Memory();

        Byte * const cur_data_ptr = cur_page->getData() + msg_offs;

        Size const first_len = page_size - msg_offs;
        if (msg_len <= first_len) {
            cur_page = NULL;
            msg_len  = 0;
            msg_offs = 0;
            return Memory (cur_data_ptr, msg_len);
        }

        cur_page = cur_page->next_page;
        msg_offs = 0;
        msg_len -= first_len;
        return Memory (cur_data_ptr, first_len);
    }

    StreamingMessageReader (StreamingMessage const msg)
        : cur_page  (msg.first_page),
          msg_offs  (msg.msg_offs),
          msg_len   (msg.msg_len),
          page_size (StreamingPagePool::getInstance()->getPageSize())
    {}
};

}


#endif /* LIBMARY__STREAMING_PAGE_POOL__H__ */

