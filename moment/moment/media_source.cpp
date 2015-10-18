/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/media_source.h>


namespace Moment {

Size
MediaSource::Type::toString_ (Memory const &mem,
                              Format const & /* fmt */) const
{
    switch (value) {
        case MediaStream: return toString (mem, "MediaStream");
        case VodSource:   return toString (mem, "VodSource");
    }

    unreachable ();
    return 0;
}

}

