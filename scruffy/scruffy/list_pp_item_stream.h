/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__LIST_PP_ITEM_STREAM__H__
#define SCRUFFY__LIST_PP_ITEM_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/pp_item_stream.h>


namespace Scruffy {

using namespace M;

class ListPpItemStream : public PpItemStream
{
protected:
    class PositionMarker : public PpItemStream::PositionMarker
    {
    public:
	List< StRef<PpItem> >::Element *pp_el;
    };

    List< StRef<PpItem> >::Element *cur_pp;

    const Pargen::FilePosition start_fpos;

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

    ListPpItemStream (List< StRef<PpItem> >::Element *cur_pp,
		      const Pargen::FilePosition &start_fpos);
};

}

#endif /* SCRUFFY__LIST_PP_ITEM_STREAM_H__ */

