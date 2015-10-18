#include <moment/util_h264.h>


namespace Moment {

void dumpH264AvcNalUnits (PagePool::Page *page,
                          size_t          msg_offs,
                          size_t          msg_len)
{
    Size const msg_csum = calculateChecksumPages (page, msg_offs, msg_len);
    logD_ (_func, "msg_offs ", msg_offs, ", msg_len ", msg_len, ", msg_csum " _hex (msg_csum));

    for (;;) {
        if (msg_len == 0)
            break;

        if (msg_len < 4) {
            logD_ (_func, "WARNING: discarding ", msg_len, " bytes");
            break;
        }

        while (page->data_len <= msg_offs) {
            msg_offs -= page->data_len;
            page = page->getNextMsgPage();
        }

        PagePool::PageListArray pl_arr (page, msg_offs, msg_len);

        Byte nal_len_buf [4];
        pl_arr.get (/*offset=*/ 0, Memory::forObject (nal_len_buf));
        Uint32 const nal_len = readBe32 (nal_len_buf);

        if (msg_offs + 4 < msg_offs) {
            logD_ (_func, "WARNING: integer overflow, msg_offs ", msg_offs);
            break;
        }
        msg_offs += 4;

        while (page->data_len <= msg_offs) {
            msg_offs -= page->data_len;
            page = page->getNextMsgPage();
        }

        msg_len -= 4;
        if (nal_len > msg_len) {
            logD_ (_func, "WARNING: invalid nal_len ", nal_len, ", msg_len ", msg_len);
            break;
        }
        msg_len -= nal_len;

        {
            assert (page->data_len > msg_offs);
            Byte const nal_type = page->getData() [msg_offs] & 0x1f;

            Size const nal_csum = calculateChecksumPages (page, msg_offs, nal_len);
            logD_ (_func, "NAL unit: nal_type ", nal_type, ", nal_len ", nal_len, ", nal_csum 0x", fmt_hex, nal_csum);
        }

        if (msg_offs + nal_len < msg_offs) {
            logD_ (_func, "WARNING: integer overflow, msg_offs ", msg_offs, ", nal_len ", nal_len);
            break;
        }
        msg_offs += nal_len;
    }
}

}

