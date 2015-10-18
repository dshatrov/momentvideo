/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>


namespace M {

ConstMemory
_libMary_stripFuncFilePath (char const * const str)
{
    if (!str)
        return ConstMemory ();

    size_t const len = strlen (str);

    size_t beg = 0;
    for (size_t i = len; i > 0; --i) {
        if (str [i - 1] == '/') {
            beg = i;
            break;
        }
    }

    if (len - beg >= 4
        && equal (ConstMemory (str + len - 4, 4), ".cpp"))
    {
        return ConstMemory (str + beg, len - beg - 4);
    }

    return ConstMemory (str + beg, len - beg);
}

Size
Result::toString_ (Memory const &mem,
                   Format const & /* fmt */) const
{
    switch (value) {
        case Success: return toString (mem, "Success");
        case Failure: return toString (mem, "Failure");
    }

    unreachable ();
    return 0;
}

}

