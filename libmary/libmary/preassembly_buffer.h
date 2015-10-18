/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__PREASSEMBLY_BUFFER__H__
#define LIBMARY__PREASSEMBLY_BUFFER__H__


#include <libmary/types.h>


namespace M {

mt_unsafe class PreassemblyBuffer
{
public:
    struct Frontend
    {
        void (*data) (ConstMemory  mem,
                      bool         limit_overflow,
                      void        *cb_data);

        void (*dataTrail) (ConstMemory  mem,
                           bool         last,
                           void        *cb_data);
    };

private:
    mt_const Size preassembly_limit;

    mt_const Frontend const *frontend;
    mt_const void *frontend_data;

    Byte *preassembly_buf;
    Size  preassembly_buf_size;
    Size  preassembled_len;

    Size content_length;

public:
    void reset (Size const content_length)
    {
        this->content_length = content_length;
        this->preassembled_len = 0;
    }

    void processData (ConstMemory mem,
                      bool        last);

    mt_const void init (Size             const preassembly_limit,
                        Frontend const * const frontend,
                        void           * const frontend_data)
    {
        this->preassembly_limit = preassembly_limit;
        this->frontend = frontend;
        this->frontend_data = frontend_data;
    }

    PreassemblyBuffer ()
        : preassembly_buf      (NULL),
          preassembly_buf_size (0),
          preassembled_len     (0),
          content_length       (0)
    {}

    ~PreassemblyBuffer ()
    {
        delete[] preassembly_buf;
    }
};

}


#endif /* LIBMARY__PREASSEMBLY_BUFFER__H__ */

