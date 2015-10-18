#include <libmary/streaming_page_pool.h>


namespace M {

mt_const StreamingPagePool* StreamingPagePool::page_pool_instance = NULL;

void
StreamingPagePool::doPageUnref (StreamingPage * const mt_nonnull page)
{
    if (page->refcnt.fetch_sub (1, std::memory_order_release) != 1)
        return;

    bool do_delete = true;
    mutex.lock ();
    if (!delete_immediately || num_spare_pages < min_pages) {
        do_delete = false;

        ++num_spare_pages;
        page->next_page = first_spare_page;
        first_spare_page = page;
    }
    mutex.unlock ();

    if (do_delete) {
        page->~StreamingPage();
        delete[] (Byte*) page;
    }
}

void
StreamingMessage::msgRef ()
{
    StreamingPage *page = first_page;
    Size left = msg_len + msg_offs;
    if (left == 0)
        return;

    Size const page_size = StreamingPagePool::getInstance()->getPageSize();

    for (;;) {
        page->pageRef ();
        if (left <= page_size)
            break;

        left -= page_size;
        page = page->next_page;
    }
}

void
StreamingPagePool::doMsgUnref (StreamingMessage const * const msg)
{
    StreamingPage *first_page_to_release = NULL;
    StreamingPage *last_page_to_release  = NULL;
    Count num_pages_to_release = 0;

    {
        Size left = msg->msg_len + msg->msg_offs;
        if (left == 0)
            return;

        Size const page_size = StreamingPagePool::getInstance()->getPageSize();

        StreamingPage *page = msg->first_page;
        for (;;) {
            // Accessing page->next_page for the very last page of the message could be a data race.
            StreamingPage * const next_page = (left > page_size ? page->next_page : NULL);

            if (page->refcnt.fetch_sub (1, std::memory_order_release) == 1) {
                // atomic_thread_fence is not available in g++-4.6
                // std::atomic_thread_fence (std::memory_order_acquire);
                page->refcnt.load (std::memory_order_acquire);

                page->next_page = first_page_to_release;
                first_page_to_release = page;
                if (!last_page_to_release) {
                    last_page_to_release = page;
                }
                ++num_pages_to_release;
            }

            if (left <= page_size)
                break;

            left -= page_size;
            page = next_page;
        }
        last_page_to_release = page;
    }

    if (!first_page_to_release)
        return;

    bool do_delete = true;
    mutex.lock ();
    if (!delete_immediately || num_spare_pages < min_pages) {
        // We do not care if we overshoot when (min_pages - num_spare_pages < num_pages_to_release),
        // because being releasing the mutex quickly is more important than single-message
        // memory management precision.

        do_delete = false;

        num_spare_pages += num_pages_to_release;
        last_page_to_release->next_page = first_spare_page;
        first_spare_page = first_page_to_release;
    }
    mutex.unlock ();

    if (do_delete) {
        StreamingPage *page = first_page_to_release;
        for (;;) {
            StreamingPage * const next_page = page->next_page;

            page->~StreamingPage ();
            delete[] (Byte*) page;

            if (page == last_page_to_release)
                break;

            page = next_page;
        }
    }
}

void
StreamingPagePool::deleteTimerTick (void * const _self)
{
    StreamingPagePool * const self = static_cast <StreamingPagePool*> (_self);
    Count num_pages_to_release = 0;

    self->mutex.lock ();

    StreamingPage * const first_page_to_release = self->first_spare_page;
    {
        StreamingPage *first_page_to_keep = first_page_to_release;
        for (Size const num_pages_to_release_end = self->delete_bulk_size;
             num_pages_to_release < num_pages_to_release_end;
             ++num_pages_to_release)
        {
            if (   first_page_to_keep == NULL
                || num_pages_to_release >= self->delete_bulk_size)
            {
                break;
            }

            first_page_to_keep = first_page_to_keep->next_page;
        }

        assert (self->num_spare_pages >= num_pages_to_release);
        self->num_spare_pages -= num_pages_to_release;

        self->first_spare_page = first_page_to_keep;
    }

    self->mutex.unlock ();

    {
        StreamingPage *page = first_page_to_release;
        for (Size i = 0; i < num_pages_to_release; ++i) {
            StreamingPage * const next_page = page->next_page;

            page->~StreamingPage();
            delete[] (Byte*) page;

            page = next_page;
        }
    }
}

StreamingPage*
StreamingPagePool::getPages (Size            len,
                             StreamingPage ** const ret_last_page)
{
    if (len == 0) {
        if (ret_last_page)
            *ret_last_page = NULL;

        return NULL;
    }

    Size const page_size = StreamingPagePool::getInstance()->getPageSize();

    mutex.lock ();
    StreamingPage *first_page = first_spare_page;
    StreamingPage *last_page  = first_page;
    for (;;) {
        StreamingPage * const page = first_spare_page;
        if (!page)
            break;

        first_spare_page = page->next_page;
        last_page = page;
        --num_spare_pages;

        if (len <= page_size) {
            len = 0;
            break;
        }

        len -= page_size;
    }
    mutex.unlock ();

    if (len) {
        for (;;) {
            Byte * const buf = new (std::nothrow) Byte [sizeof (StreamingPage) + page_size];
            assert (buf);
            StreamingPage * const page = new (buf) StreamingPage (/*refcnt=*/ 1);
            assert (page);

            last_page->next_page = page;
            last_page = page;

            if (len <= page_size)
                break;

            len -= page_size;
        }
    }

    if (ret_last_page)
        *ret_last_page = last_page;

    return first_page;
}

mt_const void
StreamingPagePool::init (Size     const page_size,
                         Count    const min_pages,
                         bool     const delete_immediately,
                         Timers * const timers,
                         Time     const delete_interval_microsec,
                         Count    const delete_bulk_size)
{
    this->page_size          = page_size;
    this->min_pages          = min_pages;
    this->delete_immediately = delete_immediately;
    this->delete_bulk_size   = delete_bulk_size;

    {
        StreamingPage *prv_page = NULL;
        for (Count i = 0; i < min_pages; ++i) {
            Byte * const buf = new (std::nothrow) Byte [sizeof (StreamingPage) + page_size];
            assert (buf);
            StreamingPage * const page = new (buf) StreamingPage (/*refcnt=*/ 0);
            assert (page);

            if (prv_page)
                prv_page->next_page = page;
            else
                first_spare_page = page;

            prv_page = page;
        }
        if (prv_page)
            prv_page->next_page = NULL;
    }
    num_spare_pages = min_pages;

    if (!delete_immediately) {
        assert (timers);
        timers->addTimer_microseconds (
                CbDesc<Timers::TimerCallback> (deleteTimerTick, this, this),
                delete_interval_microsec,
                /*periodical=*/ true,
                /*auto_delete=*/ true,
                /*delete_after_tick=*/ false);
    }
}

void
StreamingPagePool::createInstance (Size     const page_size,
                                   Count    const min_pages,
                                   bool     const delete_immediately,
                                   Timers * const timers,
                                   Time     const delete_interval_microsec,
                                   Count    const delete_bulk_size)
{
    assert (!page_pool_instance);

    page_pool_instance = new StreamingPagePool (/*embed_container=*/ NULL);
    assert (page_pool_instance);

    page_pool_instance->init (page_size,
                              min_pages,
                              delete_immediately,
                              timers,
                              delete_interval_microsec,
                              delete_bulk_size);
}

StreamingPagePool::StreamingPagePool (EmbedContainer * const embed_container)
    : Object (embed_container),
      page_size          (0),
      min_pages          (0),
      delete_bulk_size   (0),
      delete_immediately (true),
      first_spare_page   (NULL),
      num_spare_pages    (0)
{
}

StreamingPagePool::~StreamingPagePool ()
{
    mutex.lock ();

    StreamingPage *page = first_spare_page;
    while (page) {
        StreamingPage * const next_page = page->next_page;

        page->~StreamingPage();
        delete[] (Byte*) page;

        page = next_page;
    }

    mutex.unlock ();
}

StreamingPage*
StreamingGetPagesContext::getPages (Size   const len,
                                    Size * const mt_nonnull ret_offs)
{
    *ret_offs = cur_offs;

    Size const page_size = StreamingPagePool::getInstance()->getPageSize();

    StreamingPage *first_page = cur_page;
    StreamingPage *last_page;

    if (!first_page) {
        first_page = StreamingPagePool::getInstance()->getPages (len, &last_page);
    } else {
        first_page = cur_page;

        Size const available_in_first_page = page_size - cur_offs;
        if (len > available_in_first_page) {
            first_page->next_page = StreamingPagePool::getInstance()->getPages (len - available_in_first_page, &last_page);
        } else {
            first_page->next_page = NULL; // not strictly necessary, present for extra safety
            last_page = first_page;
        }
    }

    cur_offs = (len + cur_offs) % page_size;
    if (cur_offs != 0) {
        cur_page = last_page;
        last_page->pageRef ();
    } else {
        cur_page = NULL;
    }

    return first_page;
}

void
StreamingFillPagesContext::fillPages (ConstMemory const mem)
{
    Size const page_size = StreamingPagePool::getInstance()->getPageSize();

    Size left = mem.len();
    if (left == 0)
        return;

    for (;;) {
        Size const left_in_page = page_size - cur_offs;
        if (left < left_in_page) {
            memcpy (cur_page->getData() + cur_offs, mem.buf() + (mem.len() - left), left);
            cur_offs += left;
            cur_len  += left;
            break;
        }

        memcpy (cur_page->getData() + cur_offs, mem.buf() + (mem.len() - left), left_in_page);
        cur_offs = 0;
        cur_len += left_in_page;
        left    -= left_in_page;
        if (cur_len < total_len) {
            cur_page = cur_page->next_page;
        } if (cur_len == total_len) {
            cur_page = NULL;
        } else {
            abort ();
        }
    }
}

void
StreamingFillPagesContext::fillPagesFromPages (StreamingMessage const msg)
{
    Size const page_size = StreamingPagePool::getInstance()->getPageSize();

    StreamingPage *page = msg.first_page;
    Size offs = msg.msg_offs;
    Size left = msg.msg_len;

    while (left) {
        Size const left_in_page = page_size - offs;
        if (left <= left_in_page) {
            fillPages (ConstMemory (page->getData() + offs, left));
            break;
        }

        fillPages (ConstMemory (page->getData() + offs, left_in_page));
        page = page->next_page;
        offs = 0;
        left -= left_in_page;
    }
}

void
StreamingFillPagesContext::fillPagesFromArray (Array * const mt_nonnull arr,
                                               Size    const offs,
                                               Size    const len)
{
    Size const page_size = StreamingPagePool::getInstance()->getPageSize();

    Size left = len;
    if (left == 0)
        return;

    for (;;) {
        Size const left_in_page = page_size - cur_offs;
        if (left < left_in_page) {
            arr->get (len - left + offs, Memory (cur_page->getData() + cur_offs, left));
            cur_offs += left;
            cur_len  += left;
            break;
        }

        arr->get (len - left + offs, Memory (cur_page->getData() + cur_offs, left_in_page));
        cur_offs = 0;
        cur_len += left_in_page;
        left    -= left_in_page;
        if (cur_len < total_len) {
            cur_page = cur_page->next_page;
        } if (cur_len == total_len) {
            cur_page = NULL;
        } else {
            abort ();
        }
    }
}

}

