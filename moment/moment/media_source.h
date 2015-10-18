/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MEDIA_SOURCE__H__
#define MOMENT__MEDIA_SOURCE__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class MediaSource : public Object
{
  public:
    struct Type {
        enum Value {
            MediaStream,
            VodSource
        };
        operator Value () const { return value; }
        Type (Value const value) : value (value) {}
        Type () {}
        Size toString_ (Memory const &mem, Format const &fmt) const;
      private:
        operator bool () const; // forbidden
        Value value;
    };

    Type const type;

    MediaSource (EmbedContainer * const embed_container,
                 Type             const type)
        : Object (embed_container),
          type   (type)
    {}
};

}


#endif /* MOMENT__MEDIA_SOURCE__H__ */

