/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <scruffy/list_pp_item_stream.h>


using namespace M;
using namespace Pargen;

namespace Scruffy {
    
PpItemStream::PpItemResult
ListPpItemStream::getNextItem (StRef<PpItem> *pp_item)
    throw (InternalException,
	   ParsingException)
{
    if (cur_pp != NULL) {
	if (pp_item != NULL)
	    *pp_item = cur_pp->data;

	cur_pp = cur_pp->next;
	return PpItemNormal;
    }

    if (pp_item != NULL)
	*pp_item = NULL;

    return PpItemEof;
}

PpItemStream::PpItemResult
ListPpItemStream::getHeaderName (StRef<PpToken_HeaderName> * /* ret_hn_token */)
    throw (ParsingException,
	   InternalException)
{
    // TODO Is this necessary?

    // No-op

    unreachable ();

    return PpItemNormal;
}

StRef<PpItemStream::PositionMarker>
ListPpItemStream::getPosition ()
    throw (InternalException)
{
    StRef<PpItemStream::PositionMarker> ret_pmark = st_grab (static_cast <PpItemStream::PositionMarker*> (new (std::nothrow) PositionMarker));

    PositionMarker *pmark = static_cast <PositionMarker*> (ret_pmark.ptr ());
    pmark->pp_el = cur_pp;

    return ret_pmark;
}

void
ListPpItemStream::setPosition (PpItemStream::PositionMarker *_pmark)
    throw (InternalException)
{
    assert (_pmark);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    cur_pp = pmark->pp_el;
}

FilePosition
ListPpItemStream::getFpos (PpItemStream::PositionMarker *_pmark)
{
    assert (_pmark == NULL);

    PositionMarker *pmark = static_cast <PositionMarker*> (_pmark);
    if (pmark->pp_el != NULL)
	return pmark->pp_el->data->fpos;

    return start_fpos;
}

FilePosition
ListPpItemStream::getFpos ()
{
    if (cur_pp != NULL)
	return cur_pp->data->fpos;

    return start_fpos;
}

ListPpItemStream::ListPpItemStream (List< StRef<PpItem> >::Element *cur_pp,
				    const FilePosition &start_fpos)
    : start_fpos (start_fpos)
{
    this->cur_pp = cur_pp;
}

}

