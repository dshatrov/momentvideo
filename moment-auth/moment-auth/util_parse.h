/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_AUTH__UTIL_PARSE__H__
#define MOMENT_AUTH__UTIL_PARSE__H__


#include <libmary/libmary.h>


namespace MomentAuth {

using namespace M;

static bool isWhitespace (Byte const c)
{
    return    c == '\r'
           || c == '\n'
           || c == ' '
           || c == '\t';
}

static bool isAlphanumeric (Byte const c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            (c == '-') ||
            (c == '_'));
}

static void skipWhitespace (Byte ** const mt_nonnull ret_buf,
                            Size  * const mt_nonnull ret_len)
{
    Byte *buf = *ret_buf;
    Size  len = *ret_len;

    while (len) {
        if (!isWhitespace (*buf))
            break;

        ++buf;
        --len;
    }

    *ret_buf = buf;
    *ret_len = len;
}

/* Unused
static void skipToWhitespace (Byte ** const mt_nonnull ret_buf,
                              Size  * const mt_nonnull ret_len)
{
    Byte *buf = *ret_buf;
    Size  len = *ret_len;

    while (len) {
        if (isWhitespace (*buf))
            break;

        ++buf;
        --len;
    }

    *ret_buf = buf;
    *ret_len = len;
}
*/

static void skipToAfterSymbol (Byte ** const mt_nonnull ret_buf,
                               Size  * const mt_nonnull ret_len,
                               Byte    const c)
{
    Byte *buf = *ret_buf;
    Size  len = *ret_len;

    while (len) {
        Byte const cur_c = *buf;

        ++buf;
        --len;

        if (cur_c == c)
            break;
    }

    *ret_buf = buf;
    *ret_len = len;
}

static void skipToAlphanumeric (Byte ** const mt_nonnull ret_buf,
                                Size  * const mt_nonnull ret_len)
{
    Byte *buf = *ret_buf;
    Size  len = *ret_len;

    while (len) {
        if (isAlphanumeric (*buf))
            break;

        ++buf;
        --len;
    }

    *ret_buf = buf;
    *ret_len = len;
}

static void skipToNonAlphanumeric (Byte ** const mt_nonnull ret_buf,
                                   Size  * const mt_nonnull ret_len)
{
    Byte *buf = *ret_buf;
    Size  len = *ret_len;

    while (len) {
        if (!isAlphanumeric (*buf))
            break;

        ++buf;
        --len;
    }

    *ret_buf = buf;
    *ret_len = len;
}

}


#endif /* MOMENT_AUTH__UTIL_PARSE__H__ */

