/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFYY__UNICHAR_PP_ITEM_STREAM__H__
#define SCRUFYY__UNICHAR_PP_ITEM_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/unichar_stream.h>

#include <scruffy/pp_item_stream.h>
#include <scruffy/preprocessor.h>


namespace Scruffy {

using namespace M;

class UnicharPpItemStream : public PpItemStream
{
protected:
    class PositionMarker : public PpItemStream::PositionMarker
    {
    public:
	StRef<UnicharStream::PositionMarker> unichar_pmark;
    };

    mt_const StRef<UnicharStream> unichar_stream;
    mt_const CppPreprocessor::PpTokenMatchFunc pp_token_match_func;
    mt_const StRef<String> extra_nondigits;

    StRef<UnicharStream::PositionMarker> cur_pmark;

public:
  /* PpItemStream interface */

    PpItemResult getNextItem (StRef<PpItem> *pp_item)
			    throw (InternalException,
				   ParsingException);

    PpItemResult getHeaderName (StRef<PpToken_HeaderName> *ret_hn_token)
			 throw (ParsingException,
				InternalException);

    StRef<PpItemStream::PositionMarker> getPosition ()
			    throw (InternalException);

    void setPosition (PpItemStream::PositionMarker *pmark)
			    throw (InternalException);

    Pargen::FilePosition getFpos (PpItemStream::PositionMarker *pmark);

    Pargen::FilePosition getFpos ();

  /* (End of PpItemStream interface) */

    UnicharPpItemStream (UnicharStream                     *unichar_stream,
			 CppPreprocessor::PpTokenMatchFunc  pp_token_match_func,
                         ConstMemory                        extra_nondigits)
			    throw (InternalException);
};

}


#endif /* SCRUFYY__UNICHAR_PP_ITEM_STREAM__H__ */

