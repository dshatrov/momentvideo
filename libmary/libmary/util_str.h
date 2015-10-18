/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_STR__H__
#define LIBMARY__UTIL_STR__H__


#include <libmary/types.h>
#include <libmary/exception.h>
#include <libmary/file.h>

namespace M {

mt_throws Result strToInt32 (char const  *cstr,
			     Int32       *ret_val,
			     char const **ret_endptr,
			     int          base = 0);

mt_throws Result strToInt32 (ConstMemory   mem,
			     Int32        *ret_val,
			     Byte const  **ret_endptr,
			     int           base = 0);

mt_throws Result strToInt64 (char const  *cstr,
			     Int64       *ret_val,
			     char const **ret_endptr,
			     int          base = 0);

mt_throws Result strToInt64 (ConstMemory   mem,
			     Int64        *ret_val,
			     Byte const  **ret_endptr,
			     int           base = 0);

mt_throws Result strToUint32 (char const  *cstr,
			      Uint32      *ret_val,
			      char const **ret_endptr,
			      int          base = 0);

mt_throws Result strToUint32 (ConstMemory   mem,
			      Uint32       *ret_val,
			      Byte const  **ret_endptr,
			      int           base = 0);

mt_throws Result strToUint64 (char const  *cstr,
			      Uint64      *ret_val,
			      char const **ret_endptr,
			      int          base = 0);

mt_throws Result strToUint64 (ConstMemory   mem,
			      Uint64       *ret_val,
			      Byte const  **ret_endptr,
			      int           base = 0);

mt_throws Result strToInt32_safe (char const *cstr,
				  Int32 *ret_val,
				  int base = 0);

mt_throws Result strToInt32_safe (ConstMemory const &mem,
				  Int32 *ret_val,
				  int base = 0);

mt_throws Result strToInt64_safe (char const *cstr,
				  Int64 *ret_val,
				  int base = 0);

mt_throws Result strToInt64_safe (ConstMemory const &mem,
				  Int64 *ret_val,
				  int base = 0);

mt_throws Result strToUint32_safe (char const *cstr,
				   Uint32 *ret_val,
				   int base = 0);

mt_throws Result strToUint32_safe (ConstMemory const &mem,
				   Uint32 *ret_val,
				   int base = 0);

mt_throws Result strToUint64_safe (char const *cstr,
				   Uint64 *ret_val,
				   int base = 0);

mt_throws Result strToUint64_safe (ConstMemory const &mem,
				   Uint64 *ret_val,
				   int base = 0);

mt_throws Result strToDouble_safe (char const *cstr,
				   double *ret_val);

mt_throws Result strToDouble_safe (ConstMemory const &mem,
				   double *ret_val);

mt_throws StRef<Buffer> readFileToBuffer (ConstMemory filename);

static inline ConstMemory stripTrailingSlashes (ConstMemory str)
{
    while (str.len() && str.buf() [str.len() - 1] == '/')
        str = str.region (0, str.len() - 1);

    return str;
}

static inline ConstMemory stripSlashes (ConstMemory str)
{
    while (str.len() && str.buf() [0] == '/')
        str = str.region (1);

    return stripTrailingSlashes (str);
}

}


#endif /* LIBMARY__UTIL_STR__H__ */

