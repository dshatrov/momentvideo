/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/preassembly_buffer.h>


namespace M {

void
PreassemblyBuffer::processData (ConstMemory const mem,
                                bool        const last)
{
    Size const size = (content_length <= preassembly_limit ? content_length : preassembly_limit);
    Size accepted = 0;

    if (preassembled_len >= preassembly_limit) {
        if (frontend && frontend->dataTrail)
            frontend->dataTrail (mem, last, frontend_data);

        return;
    }

    bool alloc_new = true;
    if (preassembly_buf) {
        if (preassembly_buf_size >= size)
            alloc_new = false;
        else
            delete[] preassembly_buf;
    }

    if (alloc_new) {
        preassembly_buf = new (std::nothrow) Byte [size];
        assert (preassembly_buf);
        preassembly_buf_size = size;
    }

    if (mem.len() + preassembled_len >= size || last) {
        bool const limit_overflow = !last;

        {
            Size tocopy = size - preassembled_len;
            if (tocopy > mem.len())
                tocopy = mem.len();

            memcpy (preassembly_buf + preassembled_len,
                    mem.mem(),
                    tocopy);

            accepted += tocopy;
            preassembled_len += tocopy;
        }

        if (frontend && frontend->data)
            frontend->data (ConstMemory (preassembly_buf, preassembled_len), limit_overflow, frontend_data);
    }

    if (accepted < mem.len()) {
        if (frontend && frontend->dataTrail)
            frontend->dataTrail (mem.region (accepted), last, frontend_data);
    }
}

}

