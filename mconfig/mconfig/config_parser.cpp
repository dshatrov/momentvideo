/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>
#include <cctype>

#include <pargen/memory_token_stream.h>
#include <pargen/parser.h>

#include <scruffy/preprocessor.h>
#include <scruffy/list_pp_item_stream.h>
#include <scruffy/pp_item_stream_token_stream.h>
#include <scruffy/checkpoint_tracker.h>

#include <mconfig/mconfig_pargen.h>
#include <mconfig/config_parser.h>


using namespace M;

namespace MConfig {

static LogGroup libMary_logGroup_mconfig ("mconfig", LogLevel::I);

namespace {
class ConfigParser
{
  public:
    List<Section*> sections;

    Scruffy::CheckpointTracker checkpoint_tracker;

    ConfigParser (Config * const config)
    {
	sections.append (config->getRootSection());
    }
};
}

// Characters banned from words: " { } , ; # =
//
// " is used for string literals, which are provided by pargen with quotes
// included.
//
bool
mconfig_word_token_match_func (ConstMemory const &token_mem,
			       void * const /* token_user_ptr */,
			       void * const /* user_data */)
{
    logD (mconfig, _func, ConstMemory (token_mem));

    if (token_mem.len() == 0)
	return false;

    char const * const token = (char const*) token_mem.mem();
    if (token [0] == '{' ||
	token [0] == '}' ||
	token [0] == ',' ||
	token [0] == ';' ||
	token [0] == '#' ||
	token [0] == '=')
    {
	return false;
    }

    if (token [0] == '"'
	&& (token_mem.len() == 1
	    || token [token_mem.len() - 1] != '"'))
    {
	return false;
    }

    return true;
}

static bool
whitespaceBetweenWordsNeeded (ConstMemory const &left,
			      ConstMemory const &right)
{
    if (left .len() == 0 ||
	right.len() == 0)
    {
	return false;
    }

    if (isalnum (left.mem() [left.len() - 1]) &&
	isalnum (right.mem() [0]))
    {
	return true;
    }

    return false;
}

static StRef<String>
wordsToString (IntrusiveList<MConfig_Word> * const mt_nonnull words)
{
    Size str_len = 0;
    {
	ConstMemory prv_word;
	IntrusiveList<MConfig_Word>::iterator iter (*words);
	while (!iter.done ()) {
	    MConfig_Word * const word_ = iter.next();

	    ConstMemory word = word_->any_token->token;
	    // Stripping quotes from string literals.
	    if (word.len() && word.mem() [0] == '"') {
		assert (word.len() >= 2);
		word = word.region (1, word.len() - 2);
	    }

	    str_len += word.len();

	    if (whitespaceBetweenWordsNeeded (prv_word, word))
		++str_len;

	    prv_word = word;
	}
    }

    logD (mconfig, _func, "str_len: ", str_len);

    StRef<String> str = st_grab (new (std::nothrow) String (str_len));

    {
	Size pos = 0;
	ConstMemory prv_word;
	IntrusiveList<MConfig_Word>::iterator iter (*words);
	while (!iter.done ()) {
	    MConfig_Word * const word_ = iter.next();

	    ConstMemory word = word_->any_token->token;
	    logD (mconfig, _func, "word: ", word);
	    // Stripping quotes from string literals.
	    if (word.len() && word.mem() [0] == '"') {
		assert (word.len() >= 2);
		word = word.region (1, word.len() - 2);
	    }

	    if (whitespaceBetweenWordsNeeded (prv_word, word)) {
		str->mem().mem() [pos] = ' ';
		++pos;
	    }

	    memcpy (str->mem().mem() + pos, word.mem(), word.len());
	    pos += word.len();

	    prv_word = word;
	}
    }

    logD (mconfig, _func, "str: ", str);

    return str;
}

static StRef<String>
keyToString (MConfig_Key * const key)
{
    if (!key)
	return st_grab (new (std::nothrow) String);

    return wordsToString (&key->words);
}

bool
mconfig_begin_section (MConfig_Section       * const section,
		       Pargen::ParserControl * const /* parser_control */,
		       void                  * const _self)
{
    ConfigParser * const self = static_cast <ConfigParser*> (_self);

//    StRef<String> const section_name = keyToString (section->name);
    ConstMemory const section_name = section->name ? ConstMemory (section->name->any_token->token) : ConstMemory();

    logD (mconfig, _func, "section: ", section_name);

// Section value replacement was disabled to allow lists of sections
// with the same name (for Moment's mod_file).
//  Section *opts_section = self->sections.getLast()->getSection (section_name->mem());
    Section *opts_section = NULL;
    if (!opts_section) {
//	opts_section = new Section (keyToString (section->name)->mem());
	opts_section = new (std::nothrow) Section (section_name);
        assert (opts_section);
	self->sections.getLast()->addSection (opts_section);
    }

    {
        IntrusiveList<MConfig_Attribute>::iterator iter (section->attributes);
        while (!iter.done()) {
            MConfig_Attribute * const attr = iter.next();

            ConstMemory attr_name;
            ConstMemory attr_value;
            bool has_value = false;

            switch (attr->attribute_type) {
                case MConfig_Attribute::t_NameValue: {
                    MConfig_Attribute_NameValue * const attr__name_value =
                            static_cast <MConfig_Attribute_NameValue*> (attr);
                    has_value = true;
                    if (attr__name_value->name)
                        attr_name = attr__name_value->name->any_token->token;
                    if (attr__name_value->value)
                        attr_value = attr__name_value->value->any_token->token;
                } break;
                case MConfig_Attribute::t_Name: {
                    MConfig_Attribute_Name * const attr__name =
                            static_cast <MConfig_Attribute_Name*> (attr);
                    if (attr__name->name)
                        attr_name = attr__name->name->any_token->token;
                } break;
                default:
                    unreachable ();
            }

            Attribute *opts_attr = opts_section->getAttribute (attr_name);
            if (opts_attr) {
                opts_attr->setValue (has_value, attr_value);
            } else {
                opts_attr = new (std::nothrow) Attribute (attr_name, has_value, attr_value);
                assert (opts_attr);
                opts_section->addAttribute (opts_attr);
            }
        }
    }

    self->sections.append (opts_section);
    self->checkpoint_tracker.addUnconditionalCancellable (
	    st_grab (new (std::nothrow) Scruffy::Cancellable_ListElement<Section*> (
		    self->sections,
		    self->sections.last)));

    return true;
}

bool
mconfig_accept_option (MConfig_Option         * const option,
		       Pargen::ParserControl  * const /* parser_control */,
		       void                   * const _self)
{
    ConfigParser * const self = static_cast <ConfigParser*> (_self);

    if (self->sections.isEmpty()) {
	logE_ (_func, "option not within a section");
	return false;
    }

    Section * const section = self->sections.getLast();

    MConfig_Option_KeyValue *option__key_value = NULL;

    MConfig_Key *key_elem = NULL;
    switch (option->option_type) {
	case MConfig_Option::t_KeyValue: {
	    option__key_value = static_cast <MConfig_Option_KeyValue*> (option);
	    key_elem = option__key_value->key;
	} break;
	case MConfig_Option::t_Key: {
	    MConfig_Option_Key * const option__key = static_cast <MConfig_Option_Key*> (option);
	    key_elem = option__key->key;
	} break;
	default:
	    unreachable ();
    }

    StRef<String> key = keyToString (key_elem);

    logD (mconfig, _func, "option: ", key);

    Ref<Option> opts_option = section->getOption (key->mem());
    if (opts_option) {
	opts_option->removeValues ();
    } else {
	opts_option = grab (new (std::nothrow) Option (key->mem()));
	self->sections.getLast()->addOption (opts_option);
    }

    if (option__key_value) {
	MConfig_Value *value = option__key_value->value;
	for (;;) {
	    switch (value->value_type) {
		case MConfig_Value::t_List: {
		    MConfig_Value_List * const value__list = static_cast <MConfig_Value_List*> (value);
		    logD (mconfig, _func, "value: ", wordsToString (&value__list->words));
		    opts_option->addValue (wordsToString (&value__list->words)->mem());
		    value = value__list->value;
		    logD (mconfig, _func, "new value: 0x", fmt_hex, (UintPtr) value);
		} break;
		case MConfig_Value::t_Word: {
		    MConfig_Value_Word * const value__word = static_cast <MConfig_Value_Word*> (value);
		    logD (mconfig, _func, "value: ", wordsToString (&value__word->words));
		    opts_option->addValue (wordsToString (&value__word->words)->mem());
		    value = NULL;
		} break;
		default:
		    unreachable ();
	    }

	    if (!value)
		break;
	}
    }

    logD (mconfig, _func, "done");

    return true;
}

Result parseConfig (ConstMemory   const filename,
		    Config      * const config)
{
//    logD_ (_func, "filename: ", filename);

 #ifndef LIBMARY_NO_EXCEPTIONS
  try {
 #endif
    StRef<Pargen::Grammar> const grammar = create_mconfig_grammar ();

    NativeFile native_file (NULL /* embed_container */);
    if (!native_file.open (filename, 0 /* open_flags */, FileAccessMode::ReadOnly)) {
        logE_ (_func, "Could not open ", filename, ": ", exc->toString());
        return Result::Failure;
    }

    CachedFile file;
    Ref<PagePool> const page_pool = grab (new (std::nothrow) PagePool (NULL /* embed_container */));
    page_pool->init (4096 /* page_size */,
                     0    /* min_pages */,
                     true /* delete_immediately */,
                     NULL /* timers */,
                     0    /* delete_interval_microsec */,
                     0    /* delete_bulk_size */);
    file.init (&native_file, page_pool, 16 /* max_pages */);

    StRef<Scruffy::CppPreprocessor> const preprocessor =
            st_grab (new (std::nothrow) Scruffy::CppPreprocessor (&file, "." /* extra_nondigits */));
    if (!preprocessor->performPreprocessing ()) {
        logE_ (_func, "Preprocessing failed: ", exc->toString());
        return Result::Failure;
    }

    StRef< List_< StRef<Scruffy::PpItem>, StReferenced > > const pp_items = preprocessor->getPpItems ();
    {
        // Adding extra semicolon to allow input without trailing newline.
        pp_items->append (st_grab (new (std::nothrow) Scruffy::PpToken (
                Scruffy::PpTokenPpOpOrPunc, makeString (";"), NULL, Pargen::FilePosition())));
    }

    StRef<Scruffy::PpItemStream> const pp_stream =
	    st_grab (static_cast <Scruffy::PpItemStream*> (
		    new (std::nothrow) Scruffy::ListPpItemStream (pp_items->first,
                                                                  Pargen::FilePosition ())));

    Scruffy::PpItemStreamTokenStream token_stream_ (pp_stream);
    Scruffy::PpItemStreamTokenStream * const token_stream = &token_stream_;

    token_stream->setNewlineReplacement (";");

    ConfigParser config_parser (config);

    StRef<StReferenced> mconfig_elem_container;
    Pargen::ParserElement *mconfig_elem = NULL;
    Pargen::parse (token_stream,
		   &config_parser.checkpoint_tracker,
		   &config_parser,
		   grammar,
		   &mconfig_elem,
                   &mconfig_elem_container,
		   "default",
		   // TODO Set 'upwards_jumps' to true for non-AST mode.
		   Pargen::createParserConfig (false /* upwards_jumps */),
		   false /* debug_dump */);

    file.close (true /* flush_data */);

    ConstMemory token;
    if (!token_stream->getNextToken (&token)) {
        logE_ (_func, "Read error: ", exc->toString());
        return Result::Failure;
    }

    if (mconfig_elem == NULL ||
	token.len() > 0)
    {
	logE_ (_func, "Syntax error in configuration file ", filename);
        if (token.len())
            logE_ (_func, "Config processing stopped at token: ", token);

	return Result::Failure;
    }

    if (logLevelOn (mconfig, LogLevel::Debug)) {
	dump_mconfig_grammar (static_cast <MConfig_Grammar*> (mconfig_elem));
	errs->flush ();
    }

    return Result::Success;

 #ifndef LIBMARY_NO_EXCEPTIONS
  } catch (...) {
    logE_ (_func, "Exception");
    return Result::Failure;
  }
 #endif
}

mt_throws Result parseConfig_Memory_NoPreprocessor (ConstMemory   const mem,
                                                    Config      * const config)
{
 #ifndef LIBMARY_NO_EXCEPTIONS
  try {
 #endif
    StRef<Pargen::Grammar> const grammar = create_mconfig_grammar ();

    Pargen::MemoryTokenStream token_stream;
    token_stream.init (mem,
                       true  /* report_newlines */,
                       ";"   /* newline_replacement */,
                       false /* minus_is_alpha */,
                       4096  /* max_token_len */);

    ConfigParser config_parser (config);

    StRef<StReferenced> mconfig_elem_container;
    Pargen::ParserElement *mconfig_elem = NULL;
    Pargen::parse (&token_stream,
		   &config_parser.checkpoint_tracker,
		   &config_parser,
		   grammar,
		   &mconfig_elem,
                   &mconfig_elem_container,
		   "default",
		   // TODO Set 'upwards_jumps' to true for non-AST mode.
		   Pargen::createParserConfig (false /* upwards_jumps */),
		   false /* debug_dump */);

    ConstMemory token;
    if (!token_stream.getNextToken (&token)) {
        logE_ (_func, "Read error: ", exc->toString());
        exc_throw (InternalException, InternalException::BadInput);
        return Result::Failure;
    }

    if (mconfig_elem == NULL ||
	token.len() > 0)
    {
	logE_ (_func, "Syntax error in config");
        if (token.len())
            logE_ (_func, "Config processing stopped at token: ", token);

        exc_throw (InternalException, InternalException::BadInput);
	return Result::Failure;
    }
 #ifndef LIBMARY_NO_EXCEPTIONS
  } catch (...) {
      logE_ (_func, "parsing exception");
      exc_throw (InternalException, InternalException::BadInput);
      return Result::Failure;
  }
 #endif

  return Result::Success;
}

}

