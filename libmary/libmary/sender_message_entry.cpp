/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/sender_message_entry.h>


namespace M {

#ifdef LIBMARY_SENDER_VSLAB
  static VSlab<SenderMessageEntry_Pages> msg_vslab (1 << 16 /* prealloc, 64К messages */ /* TODO Preallocate less */);
  #ifdef LIBMARY_MT_SAFE
    static Mutex msg_vslab_mutex;
  #endif
#endif

Size
SenderMessageEntry_Pages::getTotalMsgLen () const
{
    return header_len + msg_len;
}

bool
SenderMessageEntry_Pages::isEmpty ()
{
    return (header_len == 0) && (msg_len == 0);
}

void
SenderMessageEntry_Pages::fillIovs (IovArray * const iovs,
                                    Size       const max_iovs)
{
    Size i = 0;

    if (header_len - header_offs > 0) {
        iovs->addIov (getHeaderData() + header_offs, header_len - header_offs);

        ++i;
        if (i >= max_iovs)
            return;
    }

    PagePool::Page *page = cur_page;
    Size left = msg_left;
    while (left) {
        assert (page);

        Size const offs = (page == cur_page ? page_offs : 0);
        Size const len = (left <= page->data_len - offs) ? left : page->data_len - offs;

        if (len > 0) {
            iovs->addIov (page->getData() + offs, len);

            ++i;
            if (i >= max_iovs)
                return;
        }

        left -= len;
        page = page->getNextMsgPage();
    }
}

bool
SenderMessageEntry_Pages::react (Size  bytes_sent,
                                 Size * const mt_nonnull ret_accepted)
{
    *ret_accepted = 0;

    if (header_len > header_offs) {
        if (bytes_sent < header_len - header_offs) {
            header_offs   += bytes_sent;
            *ret_accepted += bytes_sent;
            return false;
        }

        bytes_sent    -= (header_len - header_offs);
        *ret_accepted += (header_len - header_offs);

        header_offs = header_len;
    }

    PagePool::Page *page = cur_page;
    while (msg_left > 0) {
        assert (page);

        Size const len = (msg_left <= page->data_len - page_offs) ? msg_left : page->data_len - page_offs;

        if (bytes_sent < len) {
            page_offs     += bytes_sent;
            msg_left      -= bytes_sent;
            *ret_accepted += bytes_sent;
            return false;
        }

        bytes_sent    -= len;
        msg_left      -= len;
        *ret_accepted += len;

        PagePool::Page * const next_page = page->getNextMsgPage();

        page_offs = 0;
        cur_page = next_page;
      #ifndef LIBMARY_WIN32_IOCP
        page_pool->pageUnref (page);
      #endif

        page = next_page;
    }

    return msg_left == 0;
}

void
SenderMessageEntry_Pages::release ()
{
  #ifdef LIBMARY_WIN32_IOCP
    PagePool::Page *page = first_page;
    Size left = msg_len;
    Size offs = msg_offs;
  #else
    PagePool::Page *page = cur_page;
    Size left = msg_left;
    Size offs = page_offs;
  #endif
    if (left > 0) {
        for (;;) {
            assert (page);
            PagePool::Page * const next_page = page->getNextMsgPage();
            Size const len = page->data_len - offs;

            page_pool->pageUnref (page);

            if (left <= len)
                break;

            left -= len;
            page = next_page;
            offs = 0;
        }
    }

  #ifdef LIBMARY_SENDER_VSLAB
    if (vslab_key) {
      #ifdef LIBMARY_MT_SAFE
        msg_vslab_mutex.lock ();
      #endif
        VSlab<SenderMessageEntry_Pages>::AllocKey tmp_vslab_key = vslab_key;
        this->~SenderMessageEntry_Pages ();
        msg_vslab.free (tmp_vslab_key);
      #ifdef LIBMARY_MT_SAFE
        msg_vslab_mutex.unlock ();
      #endif
    } else {
        this->~SenderMessageEntry_Pages ();
        delete[] (Byte*) this;
    }
  #else
    this->~SenderMessageEntry_Pages ();
    delete[] (Byte*) this;
  #endif
}

SenderMessageEntry_Pages* SenderMessageEntry_Pages::createNew (Size const header_len)
{
  #ifdef LIBMARY_SENDER_VSLAB
    unsigned const vslab_header_len = 33 /* RtmpConnection::MaxHeaderLen */;
    if (header_len <= vslab_header_len /* TODO Artificial limit (matches Moment::RtmpConnection's needs) */) {
        VSlab<SenderMessageEntry_Pages>::AllocKey vslab_key;
        SenderMessageEntry_Pages *msg_pages;
        {
          #ifdef LIBMARY_MT_SAFE
            msg_vslab_mutex.lock ();
          #endif
            msg_pages = msg_vslab.alloc (sizeof (SenderMessageEntry_Pages) + vslab_header_len, &vslab_key);
            new (msg_pages) SenderMessageEntry_Pages;
          #ifdef LIBMARY_MT_SAFE
            msg_vslab_mutex.unlock ();
          #endif
        }
        msg_pages->vslab_key = vslab_key;
        return msg_pages;
    } else {
        Byte * const buf = new (std::nothrow) Byte [sizeof (SenderMessageEntry_Pages) + header_len];
        assert (buf);
        SenderMessageEntry_Pages * const msg_pages = new (buf) SenderMessageEntry_Pages;
        msg_pages->vslab_key = NULL;
        return msg_pages;
    }
  #else
    Byte * const buf = new (std::nothrow) Byte [sizeof (SenderMessageEntry_Pages) + header_len];
    assert (buf);
    return new (buf) SenderMessageEntry_Pages;
  #endif
}

}

