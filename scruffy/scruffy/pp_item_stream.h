/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__PP_ITEM_STREAM__H__
#define SCRUFFY__PP_ITEM_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/file_position.h>

#include <scruffy/preprocessor_types.h>
#include <scruffy/parsing_exception.h>


namespace Scruffy {

using namespace M;

class PpItemStream : public StReferenced
{
public:
    class PositionMarker : public StReferenced
    {
    };

    enum PpItemResult {
	PpItemNormal = 0,
	PpItemEof
    };

  /* Virtual methods */

    /* If 'PpTokenNormal' is returned, then it is guaranteed that
     * '*pp_item' is non-null and contains the next item. */
    virtual PpItemResult getNextItem (StRef<PpItem> *pp_item)
			    throw (InternalException,
				   ParsingException) = 0;

    // header-name is a special kind of preprocessing token which
    // can occur only in an #include directive. Hence we need
    // a context-dependent hook for parsing it.
    //
    // If PpItemNormal is returned and ret_hn_token is non-null,
    // then the value of ret_hn_token determines if there was a match
    // after the call (if null, then no match).
    virtual PpItemResult getHeaderName (StRef<PpToken_HeaderName> *ret_hn_token)
				 throw (ParsingException,
					InternalException) = 0;

    virtual StRef<PositionMarker> getPosition ()
				      throw (InternalException) = 0;

    virtual void setPosition (PositionMarker *pmark)
		       throw (InternalException) = 0;

    virtual Pargen::FilePosition getFpos (PositionMarker *pmark) = 0;

    virtual Pargen::FilePosition getFpos () = 0;

  /* (End of virtual methods) */

    /* 'whsp' is set to null if there is any non-whitespace character
     * in the way. */
    PpItemResult getWhitespace (StRef<Whitespace> *whsp)
			 throw (InternalException,
				ParsingException);

    /* 'pp_token' is set to null if there is something in the way
     * which is not a preprocessing token. */
    PpItemResult getPpToken (StRef<PpToken> *pp_token)
		      throw (InternalException,
			     ParsingException);

    /* Skips all whitespace before the next preprocessing token.
     *
     * If 'PpTokenNormal' is returned, then it is guaranteed that
     * '*pp_token' is non-null and contains the next preprocessing token.
     * Otherwise it is guaranteed that '*pp_token' will be set to NULL
     * (as long as 'pp_token' is not NULL). */
    PpItemResult getNextPpToken (StRef<PpToken> *pp_token)
			  throw (InternalException,
				 ParsingException);
};

}


#endif /* SCRUFFY__PP_ITEM_STREAM__H__ */

