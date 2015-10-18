/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_DEV__H__
#define LIBMARY__UTIL_DEV__H__


#include <libmary/io.h>


namespace M {

Uint32 calculateChecksum (ConstMemory mem);

void hexdump (OutputStream * mt_nonnull out,
              ConstMemory   prefix,
              ConstMemory   mem);

static inline void hexdump (OutputStream * const mt_nonnull out,
                            ConstMemory    const mem)
    { hexdump (out, ConstMemory(), mem); }

static inline void hexdump (ConstMemory const mem)
    { hexdump (errs, mem); }

}


#endif /* LIBMARY__UTIL_DEV__H__ */

