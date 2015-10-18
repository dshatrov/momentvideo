/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_COMMON__H__
#define LIBMARY__UTIL_COMMON__H__


#include <libmary/types.h>
#include <libmary/exception.h>


namespace M {

void randomSetSeed (Uint32 seed);

Uint32 randomUint32 ();

template <class T>
mt_throws Result writeFull_common (T * const  dest,
				   ConstMemory const &mem,
				   Size * const ret_nwritten)
{
    Size total_written = 0;
    Result res = Result::Success;

    while (total_written < mem.len()) {
	Size last_written;
	res = dest->write (mem.region (total_written, mem.len() - total_written), &last_written);
	total_written += last_written;
	if (!res)
	    break;
    }

    if (ret_nwritten)
	*ret_nwritten = total_written;

    return res;
}

}


#endif /* LIBMARY__UTIL_COMMON__H__ */

