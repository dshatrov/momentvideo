/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/page_pool.h>
#include <libmary/util_str.h>
#include <libmary/log.h>

#include <libmary/util_dev.h>


namespace M {

Uint32 calculateChecksum (ConstMemory const mem)
{
    Uint32 csum = 0;
    Byte const * const buf = mem.mem();
    for (Size i = 0, i_end = mem.len(); i < i_end; ++i) {
        csum += (Uint32) buf [i];
    }
    return csum;
}

Uint32 calculateChecksumPages (PagePool::Page * const first_page,
                               Size             const msg_offset)
{
    Uint32 csum = 0;

    PagePool::Page *page = first_page;
    while (page) {
        Size i = 0;
        if (page == first_page)
            i = msg_offset;

        for (; i < page->data_len; ++i)
            csum += (Uint32) page->getData() [i];

        page = page->getNextMsgPage();
    }

    return csum;
}

Uint64 calculateChecksumPages (PagePool::Page * const first_page,
                               Size             const msg_offs,
                               Size             const msg_len)
{
    Uint64 csum = 0;

    PagePool::Page *page = first_page;
    Size len = msg_len;
    while (len) {
        for (Size i = (page == first_page ? msg_offs : 0);
             i < page->data_len && len > 0;
             ++i)
        {
            csum += (Uint64) page->getData() [i];
            --len;
        }

        page = page->getNextMsgPage();
        assert (page || len == 0);
    }

    return csum;
}

void
hexdump (OutputStream * const mt_nonnull out,
         ConstMemory    const prefix,
         ConstMemory    const mem)
{
    static char const ascii_tab [256] = {
      //  0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 00
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 10
        '.', '!', '"', '#', '$', '%', '&', '\'','(', ')', '*', '+', ',', '-', '.', '/', // 20
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?', // 30
        '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 40
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\',']', '^', '_', // 50
        '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', // 60
        'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '.', // 70
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 80
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // 90
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // a0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // b0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // c0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // d0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // e0
        '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', '.', // f0
    };

    Count const num_rows = 16;
    Count const row_neighbours = 8;

    Byte line_buf [256 + 1];
    Memory line_mem = Memory (line_buf, sizeof (line_buf) - 1);

    for (Size i = 0; i < mem.len(); ) {
        Size offs = 0;

        offs += toString (line_mem.safeRegion (offs), "0x");
        {
            Format fmt;
            fmt.num_base = 16;
            fmt.min_digits = 4;
            offs += toString (line_mem.safeRegion (offs), i, fmt);
        }
        offs += toString (line_mem.safeRegion (offs), " |");

        size_t j_limit = mem.len() - i;
        if (j_limit > num_rows)
            j_limit = num_rows;

        size_t neigh = row_neighbours;
        for (size_t j = 0; j < num_rows; ++j) {
            if (neigh == 0) {
                offs += toString (line_mem.safeRegion (offs), " ");
                neigh = row_neighbours;
            }
            --neigh;

            if (j < j_limit) {
                offs += toString (line_mem.safeRegion (offs), " ");
                {
                    Format fmt;
                    fmt.num_base = 16;
                    fmt.min_digits = 2;
                    offs += toString (line_mem.safeRegion (offs), mem.mem() [i + j], fmt);
                }
            } else {
                offs += toString (line_mem.safeRegion (offs), "   ");
            }
        }

        offs += toString (line_mem.safeRegion (offs), " | ");

        neigh = row_neighbours;
        for (size_t j = 0; j < j_limit; ++j) {
            if (neigh == 0) {
                offs += toString (line_mem.safeRegion (offs), " ");
                neigh = row_neighbours;
            }
            --neigh;

            offs += toString (line_mem.safeRegion (offs), ConstMemory::forObject (ascii_tab [mem.mem() [i + j]]));
        }

        assert (offs < sizeof (line_buf));
        line_buf [offs] = '\n';

        if (prefix.len()) {
            out->print_ (prefix);
        }
        out->print_ (ConstMemory::forObject (line_buf).safeRegion (0, offs + 1));

        i += j_limit;
    }

    out->flush ();
}

}

