/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary_md5.h>

#include "md5/md5.h"


namespace M {

void getMd5HexAscii (ConstMemory const src,
                     Memory      const result)
{
    assert (result.len() >= 32);

    md5_state_t state;
    md5_byte_t digest [16];

    md5_init (&state);
    md5_append (&state, (md5_byte_t const *) src.mem(), (int) src.len());
    md5_finish (&state, digest);

    for (int i = 0; i < 16; ++i) {
        unsigned char const low  =  digest [i] & 0x0f;
        unsigned char const high = (digest [i] & 0xf0) >> 4;

        unsigned long const pos = i << 1;

        if (high < 10)
            result.mem() [pos] = high + '0';
        else
            result.mem() [pos] = high - 10 + 'a';

        if (low < 10)
            result.mem() [pos + 1] = low + '0';
        else
            result.mem() [pos + 1] = low - 10 + 'a';
    }
}

}

