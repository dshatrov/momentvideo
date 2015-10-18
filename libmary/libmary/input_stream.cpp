/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/util_common.h>

#include <libmary/input_stream.h>


namespace M {

mt_throws IoResult
InputStream::readFull (Memory   const mem,
		       Size   * const ret_nread)
{
    Size bread = 0;
    IoResult res = IoResult::Normal;

    while (bread < mem.len()) {
	Size last_read;
	res = read (mem.region (bread, mem.len() - bread), &last_read);
        if (res == IoResult::Error)
            break;

        if (res == IoResult::Eof) {
            if (bread > 0)
                res = IoResult::Normal;

            break;
        }

        assert (res == IoResult::Normal);

	bread += last_read;
    }

    if (ret_nread)
	*ret_nread = bread;

    return res;
}

}

