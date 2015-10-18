/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__PREPROCESSOR__H__
#define SCRUFFY__PREPROCESSOR__H__


#include <libmary/libmary.h>

#include <scruffy/unichar_stream.h>

#include <scruffy/preprocessor_types.h>
#include <scruffy/pp_item_stream.h>
#include <scruffy/parsing_exception.h>


namespace Scruffy {

using namespace M;

// mt_throws ((InternalException, ParsingException))
unsigned long matchPreprocessingToken (UnicharStream *unichar_stream,
				       PpTokenType   *ret_pp_token_type,
                                       ConstMemory    extra_nondigits);

class CppPreprocessor : public StReferenced
{
public:
    // mt_throws ((InternalException, ParsingException))
    typedef unsigned long (*PpTokenMatchFunc) (UnicharStream *unichar_stream,
					       PpTokenType   *ret_pp_token_type,
                                               ConstMemory    extra_nondigits);

public:
//protected:
    class IfStackEntry
    {
    public:
	enum State {
	    If,
	    Elif,
	    Else
	};

	State state;

	enum MatchType {
	    NoMatch,
	    Match,
	    PrvMatch,
	    Skipping
	};

	MatchType match_type;

	IfStackEntry () {}

	IfStackEntry (State state,
		      MatchType match_type)
	{
	    this->state = state;
	    this->match_type = match_type;
	}
    };

    mt_const PpTokenMatchFunc pp_token_match_func;
    mt_const StRef<String> extra_nondigits;

    List<IfStackEntry> if_stack;
    bool if_skipping;

    Map< StRef<MacroDefinition>,
	 MemberExtractor< MacroDefinition,
			  StRef<String>,
			  &MacroDefinition::name,
			  Memory,
			  AccessorExtractor< String,
					     Memory,
					     &String::mem > >,
	 MemoryComparator<> >
	    macro_definitions;

    typedef List_< StRef<PpItem>, StReferenced > PpItemList;
    StRef<PpItemList> pp_items;

    File *source_file;

    StRef< List_< StRef<String>, StReferenced > >
	    extractMacroParameters (PpItemStream *pp_stream)
			     throw (InternalException,
				    ParsingException);

    StRef< List_< StRef<PpItem>, StReferenced > >
	    extractReplacementList (PpItemStream *pp_stream)
			     throw (InternalException,
				    ParsingException);

    void do_translateIfDirective (PpItemStream *pp_stream,
				  bool elif)
			   throw (ParsingException,
				  InternalException);

    void translateIfDirective (PpItemStream *pp_stream)
			throw (ParsingException,
			       InternalException);

    void translateElifDirective (PpItemStream *pp_stream)
			  throw (ParsingException,
				 InternalException);

    void do_translateIfdefDirective (PpItemStream *pp_stream,
				     bool inverse)
			   throw (InternalException,
				  ParsingException);

    void translateIfdefDirective (PpItemStream *pp_stream)
			   throw (InternalException,
				  ParsingException);

    void translateIfndefDirective (PpItemStream *pp_stream)
			    throw (InternalException,
				   ParsingException);

    void translateElseDirective (PpItemStream *pp_stream)
			  throw (InternalException,
				 ParsingException);

    void translateEndifDirective (PpItemStream *pp_stream)
			   throw (InternalException,
				  ParsingException);

    void translateIncludeDirective (PpItemStream *pp_stream)
			     throw (ParsingException,
				    InternalException);

    void translateDefineDirective (PpItemStream *pp_stream)
			    throw (InternalException,
				   ParsingException);

    void translateUndefDirective (PpItemStream *pp_stream)
			    throw (InternalException,
				   ParsingException);

    void translatePreprocessingDirective (PpItemStream *pp_stream)
				   throw (InternalException,
					  ParsingException);

    StRef< List_< StRef<PpItem>, StReferenced > >
	    translateMacroInvocation (PpItemStream   *pp_token_stream,
				      MacroDefinition *mdef,
				      MacroBan        *macro_ban)
			       throw (InternalException,
				      ParsingException);

    StRef< List_< StRef<PpItem>, StReferenced > >
	    translateObjectMacro (MacroDefinition *mdef,
				  MacroBan        *macro_ban)
			   throw (InternalException,
				  ParsingException);

    void evaluateDoubleSharps (List< StRef<PpItem> > *pp_items,
			       MacroBan *macro_ban)
			throw (InternalException,
			       ParsingException);

    void translatePpToken (PpItemStream *pp_token_stream,
			   PpToken       *pp_token,
			   List< StRef<PpItem> > *receiving_list)
		    throw (InternalException,
			   ParsingException);

    void predefineMacros ();

    void translationPhase3 (File *in_file)
		     throw (InternalException,
			    ParsingException);

    void translationPhase6 ();

public:
    mt_throws Result performPreprocessing ();

    StRef< List_< StRef<PpItem>, StReferenced > > getPpItems () { return pp_items; }

    CppPreprocessor (File             * mt_nonnull source_file,
                     ConstMemory       extra_nondigits = ConstMemory(),
		     PpTokenMatchFunc  pp_token_match_func = matchPreprocessingToken);
};

}


#endif /* SCRUFFY__PREPROCESSOR__H__ */

