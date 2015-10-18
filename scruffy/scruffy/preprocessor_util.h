/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY_PREPROCESSOR_UTIL__H__
#define SCRUFFY_PREPROCESSOR_UTIL__H__


#include <libmary/libmary.h>

#include <scruffy/unichar_stream.h>

#include <scruffy/preprocessor_types.h>
#include <scruffy/parsing_exception.h>


namespace Scruffy {

using namespace M;

StRef<String> extractString (UnicharStream *unichar_stream,
                             unsigned long len)
                      throw (InternalException,
                             ParsingException);

unsigned long matchWhitespace (UnicharStream *unichar_stream,
			       bool *contains_newline)
			throw (InternalException,
			       ParsingException);

unsigned long matchHeaderName (UnicharStream *unichar_stream)
			throw (InternalException,
			       ParsingException);

unsigned long matchPreprocessingOpOrPunc (UnicharStream *unichar_stream)
				   throw (InternalException,
					  ParsingException);

Size matchHeaderName (UnicharStream *unichar_stream,
		      PpToken_HeaderName::HeaderNameType *ret_hn_type,
		      StRef<String> *ret_header_name)
	       throw (InternalException,
		      ParsingException);

// mt_throws ((InternalException, ParsingException))
unsigned long matchPreprocessingToken (UnicharStream *unichar_stream,
				       PpTokenType   *pp_token_type,
                                       ConstMemory    extra_nondigits);

bool compareReplacementLists (List< StRef<PpItem> > *left,
			      List< StRef<PpItem> > *right);

StRef<String> ppItemsToString (List< StRef<PpItem> > *pp_items)
                        throw (ParsingException,
                               InternalException);

StRef<String> spellPpItems (List< StRef<PpItem> > *pp_items)
                     throw (InternalException,
                            ParsingException);

StRef<String> unescapeStringLiteral (String * mt_nonnull string);

StRef<Token> ppTokenToToken (PpToken *pp_token)
                      throw (ParsingException);

void ppTokensToTokens (List< StRef<PpToken> > *pp_tokens  /* non-null */,
		       List< StRef<Token> >   *ret_tokens /* non-null */);

void ppItemsToTokens (List< StRef<PpItem> > *pp_items   /* non-null */,
		      List< StRef<Token> >  *ret_tokens /* non-null */);

}


#endif /* SCRUFFY_PREPROCESSOR_UTIL__H__ */

