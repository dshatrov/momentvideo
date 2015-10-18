/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LIBMARY_MD5__H__
#define LIBMARY__LIBMARY_MD5__H__


#include <libmary/types.h>


namespace M {

// The hash in ASCII form is always 32 characters long.
//
// @result should be at least 32 bytes long.
//
void getMd5HexAscii (ConstMemory src,
                     Memory      result);

}


#endif /* LIBMARY__LIBMARY_MD5__H__ */

