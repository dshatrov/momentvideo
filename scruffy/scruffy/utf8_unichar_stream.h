/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__UTF8_UNICHAR_STREAM__H__
#define SCRUFFY__UTF8_UNICHAR_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/unichar_stream.h>
#include <scruffy/byte_stream.h>


namespace Scruffy {

using namespace M;

class Utf8UnicharStream : public UnicharStream
{
protected:
    class PositionMarker : public UnicharStream::PositionMarker
    {
    public:
	StRef<ByteStream::PositionMarker> byte_pmark;
        Pargen::FilePosition fpos;
    };

    StRef<ByteStream> byte_stream;
    StRef<ByteStream::PositionMarker> cur_pmark;

    Pargen::FilePosition fpos;

    UnicharResult doGetNextUnichar (Unichar *ret_uc);

    unsigned skipNewline ();

public:
  mt_iface (UnicharStream)
    UnicharResult getNextUnichar (Unichar *ret_uc)
			    throw (InternalException);

    StRef<UnicharStream::PositionMarker> getPosition ()
			    throw (InternalException);

    void setPosition (UnicharStream::PositionMarker *pmark)
			    throw (InternalException);

    Pargen::FilePosition getFpos (UnicharStream::PositionMarker *_pmark);

    Pargen::FilePosition getFpos ();
  mt_iface_end

    Utf8UnicharStream (ByteStream *byte_stream)
		throw (InternalException);
};

}


#endif /* SCRUFFY__UTF8_UNICHAR_STREAM__H__ */

