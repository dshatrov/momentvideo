/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__PHASE3_ITEM_STREAM__H__
#define SCRUFFY__PHASE3_ITEM_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/pp_item_stream.h>


namespace Scruffy {

using namespace M;

class Phase3ItemStream : public PpItemStream
{
protected:
    class PositionMarker : public PpItemStream::PositionMarker
    {
    public:
	List< StRef<PpItem> >::Element *pp_el;
	StRef<PpItemStream::PositionMarker> stream_pmark;
    };

    List< StRef<PpItem> > pp_items;
    StRef<PpItemStream> pp_stream;

    List< StRef<PpItem> >::Element *cur_pp;
    StRef<PpItemStream::PositionMarker> cur_pmark;

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

    void prependItems (List< StRef<PpItem> > *items);

    void trim ();

    bool hasPrependedItems ();

    Phase3ItemStream (PpItemStream *pp_stream)
	       throw (InternalException);
};

}


#endif /* SCRUFFY__PHASE3_ITEM_STREAM__H__ */

