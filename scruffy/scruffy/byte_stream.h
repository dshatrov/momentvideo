/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__BYTE_STREAM__H__
#define SCRUFFY__BYTE_STREAM__H__


#include <libmary/libmary.h>


namespace Scruffy {

using namespace M;

class ByteStream : public StReferenced
{
public:
    class PositionMarker : public StReferenced
    {
    };

    enum ByteResult {
	ByteNormal = 0,
	ByteEof
    };

    virtual ByteResult getNextByte (char *c)
			     throw (InternalException) = 0;

    virtual StRef<PositionMarker> getPosition ()
                                        throw (InternalException) = 0;

    virtual void setPosition (PositionMarker *pmark)
                       throw (InternalException) = 0;
};

}


#endif /* SCRUFFY__BYTE_STREAM__H__ */

