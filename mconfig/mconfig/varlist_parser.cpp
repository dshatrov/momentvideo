/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents,
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>

#include <pargen/memory_token_stream.h>
#include <pargen/parser.h>

#include <mconfig/varlist_pargen.h>
#include <mconfig/varlist_parser.h>


using namespace M;

namespace MConfig {

bool
varlist_word_token_match_func (ConstMemory const &token_mem,
                               void * const /* token_user_ptr */,
                               void * const /* user_data */)
{
    char const * const token = (char const*) token_mem.mem();
    if (token [0] == '=')
        return false;

    return true;
}

bool
varlist_accept_var_decl (VarList_VarDecl        * const var_decl,
		         Pargen::ParserControl  * const /* parser_control */,
		         void                   * const _varlist)
{
    Varlist * const varlist = static_cast <Varlist*> (_varlist);

    ConstMemory name;
    ConstMemory value;
    bool with_value = false;

    bool enable_section  = false;
    bool disable_section = false;

    switch (var_decl->var_decl_type) {
        case VarList_VarDecl::t_NameValue: {
            VarList_VarDecl_NameValue * const var_decl__name_value =
                    static_cast <VarList_VarDecl_NameValue*> (var_decl);

            if (VarList_SectionSpecifier * const section_specifier =
                        var_decl__name_value->sectionSpecifier)
            {
                switch (section_specifier->section_specifier_type) {
                    case VarList_SectionSpecifier::t_Enable:
                        enable_section = true;
                        break;
                    case VarList_SectionSpecifier::t_Disable:
                        disable_section = true;
                        break;
                }
            }

            name  = var_decl__name_value->name->any_token->token;
            value = var_decl__name_value->value->any_token->token;
            with_value = true;
        } break;
        case VarList_VarDecl::t_Name: {
            VarList_VarDecl_Name * const var_decl__name =
                    static_cast <VarList_VarDecl_Name*> (var_decl);

            if (VarList_SectionSpecifier * const section_specifier =
                        var_decl__name->sectionSpecifier)
            {
                switch (section_specifier->section_specifier_type) {
                    case VarList_SectionSpecifier::t_Enable:
                        enable_section = true;
                        break;
                    case VarList_SectionSpecifier::t_Disable:
                        disable_section = true;
                        break;
                }
            }

            name  = var_decl__name->name->any_token->token;
        } break;
        default:
            unreachable ();
    }

    logD_ (_func,
           "name: ", name, ", "
           "value: ", value, ", "
           "with_value: ", with_value, ", "
           "enable_section: ", enable_section, ", "
           "disable_section: ", disable_section);

    varlist->addEntry (name, value, with_value, enable_section, disable_section);

    return true;
}

mt_throws Result
VarlistParser::parseVarlist (ConstMemory   const filename,
                             Varlist     * const varlist)
{
 #ifndef LIBMARY_NO_EXCEPTIONS
  try {
 #endif
    StRef<Buffer> const buf = mt_throws readFileToBuffer (filename);
    if (!buf)
        return Result::Failure;

    ConstMemory const mem = buf->mem;

    logD_ (_func, "varlist file data:\n", mem, "\n");

    Pargen::MemoryTokenStream token_stream;
    token_stream.init (mem,
                       true  /* report_newlines */,
                       ";"   /* newline_replacement */,
                       false /* minus_is_alpha */,
                       4096  /* max_token_len */);

    StRef<StReferenced> varlist_elem_container;
    Pargen::ParserElement *varlist_elem = NULL;
    Pargen::parse (&token_stream,
                   NULL /* lookup_data */,
                   varlist /* user_data */,
                   grammar,
                   &varlist_elem,
                   &varlist_elem_container,
                   "default",
                   parser_config,
                   false /* debug_dump */);

    ConstMemory token;
    if (!token_stream.getNextToken (&token)) {
        logE_ (_func, "read error: ", exc->toString());
        exc_throw_ (IoException);
        return Result::Failure;
    }

    if (varlist_elem == NULL ||
        token.len() > 0)
    {
        logE_ (_func, "syntax error in configuration file ", filename);
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

VarlistParser::VarlistParser ()
{
 #ifndef LIBMARY_NO_EXCEPTIONS
 try {
 #endif
    grammar = create_varlist_grammar ();
    Pargen::optimizeGrammar (grammar);
    parser_config = Pargen::createParserConfig (true /* upwards_jumps */);
 #ifndef LIBMARY_NO_EXCEPTIONS
 } catch (...) {
    logE_ (_func, "exception");
 }
 #endif
}

}

