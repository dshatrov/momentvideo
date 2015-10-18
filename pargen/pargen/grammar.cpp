/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>

#include <pargen/grammar.h>


using namespace M;

namespace Pargen {

VSlab<CompoundGrammarEntry::Acceptor> CompoundGrammarEntry::acceptor_slab;

SwitchGrammarEntry::~SwitchGrammarEntry ()
{
    TranzitionEntryHash::iter iter (tranzition_entries);
    while (!tranzition_entries.iter_done (iter)) {
	TranzitionEntry * const tranzition_entry = tranzition_entries.iter_next (iter);
	delete tranzition_entry;
    }
}

StRef<String>
Grammar_Immediate_SingleToken::toString ()
{
    return makeString ("[", token, "]");
}

StRef<String>
Grammar_Compound::toString ()
{
    return name;
}

StRef<String>
Grammar_Switch::toString ()
{
    return name;
}

}

