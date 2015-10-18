/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__BASE64__H__
#define LIBMARY__BASE64__H__


#include <libmary/types.h>


namespace M {

constexpr Size base64EncodedBufferSize (Size const decoded_size) { return decoded_size * 2 + 2; }
constexpr Size base64DecodedBufferSize (Size const encoded_size) { return encoded_size; }

Size base64Encode (ConstMemory  decoded_mem,
                   Byte        * mt_nonnull encoded_buf);

Size base64Decode (ConstMemory  encoded_mem,
                   Byte        * mt_nonnull decoded_buf);

}


#endif /* LIBMARY__BASE64__H__ */

