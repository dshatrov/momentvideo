/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__HTTP_PARSER__H__
#define LIBMARY__HTTP_PARSER__H__


#include <libmary/types.h>


namespace M {
namespace HttpParser {

// Linear white space
//
// Note that "Implied *LWS" rule in HTTP spec is just crazy.
// No sane person could come up with such a thing.
//
static inline void skipLWS (ConstMemory   const mem,
                            Size        * const mt_nonnull ret_pos)
{
    Size pos = *ret_pos;
    for (;;) {
        if (pos >= mem.len())
            return;

        if (mem.mem() [pos] == 13 /* CR */) {
            ++pos;
            if (pos == mem.len())
                return;

            if (mem.mem() [pos] == 10 /* LF */) {
                ++pos;
                if (pos == mem.len())
                    return;
            }
        }

        if (mem.mem() [pos] == 32 /* SP */ ||
            mem.mem() [pos] ==  9 /* HT */)
        {
            ++pos;
            *ret_pos = pos;
            continue;
        }

        break;
    }
}

}
}


#endif /* LIBMARY__HTTP_PARSER__H__ */

