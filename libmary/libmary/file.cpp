/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/file.h>


namespace M {

mt_throws Result
File::seekBeg (FileSize offset)
{
    SeekOrigin origin = SeekOrigin::Beg;
    while (mt_unlikely (offset > FileOffset_Max)) {
        if (!seek (FileOffset_Max, origin))
            return Result::Failure;

        offset -= FileOffset_Max;
        origin = SeekOrigin::Cur;
    }

    if (mt_likely (origin == SeekOrigin::Beg || offset > 0)) {
        if (!seek ((FileOffset) offset, origin))
            return Result::Failure;
    }

    return Result::Success;
}

}

