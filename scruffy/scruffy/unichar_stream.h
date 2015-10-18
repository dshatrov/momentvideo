/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__UNICHAR_STREAM__H__
#define SCRUFFY__UNICHAR_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/file_position.h>


namespace Scruffy {

using namespace M;

typedef char Unichar;

class UnicharStream : public StReferenced
{
public:
    class PositionMarker : public StReferenced
    {
    };

    enum UnicharResult {
	UnicharNormal = 0,
	UnicharEof
    };

  /* Virtual methods */

    virtual UnicharResult getNextUnichar (Unichar *uc)
				   throw (InternalException) = 0;

    virtual StRef<PositionMarker> getPosition ()
                                        throw (InternalException) = 0;

    virtual void setPosition (PositionMarker *pmark)
		       throw (InternalException) = 0;

    virtual Pargen::FilePosition getFpos (PositionMarker *pmark) = 0;

    virtual Pargen::FilePosition getFpos () = 0;

  /* (End of virtual methods) */

    /* Skipping means calling getNextChar() 'toskip' times.
     * If a call to getNextChar() does not result in 'UnicharNormal', then
     * an InternalException is thrown. */
    void skip (unsigned long const toskip)
	throw (InternalException)
    {
        UnicharResult ures;
        unsigned long i;
        for (i = 0; i < toskip; i++) {
            ures = getNextUnichar (NULL);
          #ifndef LIBMARY_NO_EXCEPTIONS
            if (ures != UnicharNormal)
                throw InternalException (InternalException::BadInput);
          #endif
        }
    }
};

}


#endif /* SCRUFFY__UNICHAR_STREAM__H__ */

