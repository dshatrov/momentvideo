/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>

#include <scruffy/pp_item_stream.h>


// Internal
#define DEBUG_INT(a)


using namespace M;

namespace Scruffy {
    
PpItemStream::PpItemResult
PpItemStream::getWhitespace (StRef<Whitespace> *whsp)
    throw (InternalException,
	   ParsingException)
{
    StRef<PositionMarker> pmark = getPosition ();

  DEBUG_INT (
    errf->print ("Scruffy.PpItemStream.getWhitespace: "
		 "calling getNextItem()")
	 .pendl ();
  );

    StRef<PpItem> pp_item;
    PpItemResult pres;
    pres = getNextItem (&pp_item);
    if (pres != PpItemNormal) {
	if (whsp != NULL)
	    *whsp = NULL;

	setPosition (pmark);
	return pres;
    }

    if (pp_item->type == PpItemWhitespace) {
      DEBUG_INT (
	errf->print ("Scruffy.PpItemStream.getWhitespace: "
		     "got whitespace")
	     .pendl ();
      );

	if (whsp != NULL)
	    *whsp = static_cast <Whitespace*> (pp_item.ptr ());

	return PpItemNormal;
    }

  DEBUG_INT (
    errf->print ("Scruffy.PpItemStream.getWhitespace: "
		 "got something which is not whitespace")
	 .pendl ();
  );

    if (whsp != NULL)
	*whsp = NULL;

    setPosition (pmark);
    return PpItemNormal;
}

PpItemStream::PpItemResult
PpItemStream::getPpToken (StRef<PpToken> *pp_token)
    throw (InternalException,
	   ParsingException)
{
    StRef<PositionMarker> pmark = getPosition ();

    StRef<PpItem> pp_item;
    PpItemResult pres;
    pres = getNextItem (&pp_item);
    if (pres != PpItemNormal) {
	if (pp_token != NULL)
	    *pp_token = NULL;

	setPosition (pmark);
	return pres;
    }

    if (pp_item->type == PpItemPpToken) {
	if (pp_token != NULL)
	    *pp_token = static_cast <PpToken*> (pp_item.ptr ());

	return PpItemNormal;
    }

    if (pp_token != NULL)
	*pp_token = NULL;

    setPosition (pmark);
    return PpItemNormal;
}

PpItemStream::PpItemResult
PpItemStream::getNextPpToken (StRef<PpToken> *pp_token)
    throw (InternalException,
	   ParsingException)
{
    StRef<PositionMarker> pmark = getPosition ();

    for (;;) {
	StRef<PpItem> pp_item;
	PpItemResult pres;
	pres = getNextItem (&pp_item);
	if (pres != PpItemNormal) {
	    if (pp_token != NULL)
		*pp_token = NULL;

	    setPosition (pmark);
	    return pres;
	}

	if (pp_item->type == PpItemPpToken) {
	    if (pp_token != NULL)
		*pp_token = static_cast <PpToken*> (pp_item.ptr ());

	    return PpItemNormal;
	}
    }

    unreachable ();

    if (pp_token != NULL)
	*pp_token = NULL;

    setPosition (pmark);
    return PpItemNormal;
}

}

