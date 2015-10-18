/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents,
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cctype>

#include <mconfig/util.h>


using namespace M;

namespace MConfig {

BooleanValue strToBoolean (ConstMemory const value_mem)
{
    if (value_mem.len() == 0)
	return Boolean_Default;

    Byte arr [32];
    Memory mem;
    {
        Size len = sizeof (arr);
        if (len > value_mem.len())
            len = value_mem.len();

        memcpy (arr, value_mem.buf(), len);
        mem = Memory (arr, len);
    }

    for (Size i = 0; i < mem.len(); ++i)
	mem.mem() [i] = (Byte) tolower (mem.mem() [i]);

    if (equal (mem, "y")    ||
	equal (mem, "yes")  ||
	equal (mem, "on")   ||
	equal (mem, "true") ||
	equal (mem, "1"))
    {
	return Boolean_True;
    }

    if (equal (mem, "n")     ||
	equal (mem, "no")    ||
	equal (mem, "off")   ||
	equal (mem, "false") ||
	equal (mem, "0"))
    {
	return Boolean_False;
    }

    return Boolean_Invalid;
}

}

