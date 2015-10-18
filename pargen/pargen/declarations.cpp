/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <pargen/declarations.h>


using namespace M;

namespace Pargen {

StRef<String>
PhrasePart::toString ()
{
    switch (phrase_part_type) {
	case PhrasePart::t_Phrase: {
	    PhrasePart_Phrase * const phrase_part__phrase =
		    static_cast <PhrasePart_Phrase*> (this);

            return makeString ("Phrase ", phrase_part__phrase->phrase_name);
	} break;
	case PhrasePart::t_Token: {
	    PhrasePart_Token * const phrase_part__token =
		    static_cast <PhrasePart_Token*> (this);

	    if (!phrase_part__token->token)
		return st_grab (new (std::nothrow)String ("Token *"));

            return makeString ("Token [", phrase_part__token->token, "]");
	} break;
	case PhrasePart::t_AcceptCb: {
	    PhrasePart_AcceptCb * const phrase_part__accept_cb =
		    static_cast <PhrasePart_AcceptCb*> (this);

            return makeString ("AcceptCb ", phrase_part__accept_cb->cb_name);
	} break;
	case PhrasePart::t_UniversalAcceptCb: {
	    PhrasePart_UniversalAcceptCb * const phrase_part__universal_accept_cb =
		    static_cast <PhrasePart_UniversalAcceptCb*> (this);

	    return makeString ("UniversalAcceptCb ", phrase_part__universal_accept_cb->cb_name);
	} break;
	case PhrasePart::t_UpwardsAnchor: {
	    PhrasePart_UpwardsAnchor * const phrase_part__upwards_anchor =
		    static_cast <PhrasePart_UpwardsAnchor*> (this);

	    return makeString ("UpwardsAnchor ",
                               phrase_part__upwards_anchor->declaration_name,
                               (!phrase_part__upwards_anchor->phrase_name ? ":" : ""),
                               phrase_part__upwards_anchor->phrase_name,
                               "@",
                               phrase_part__upwards_anchor->label_name);
	} break;
	case PhrasePart::t_Label: {
	    PhrasePart_Label * const phrase_part__label =
		    static_cast <PhrasePart_Label*> (this);

	    return makeString ("Label ", phrase_part__label->label_name);
	} break;
	default:
	  // No-op
	    ;
    }

    return st_grab (new (std::nothrow) String ("Unknown"));
}

}

