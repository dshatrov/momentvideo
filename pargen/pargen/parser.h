/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__PARSER__H__
#define PARGEN__PARSER__H__


#include <pargen/token_stream.h>
#include <pargen/grammar.h>
#include <pargen/parser_element.h>
#include <pargen/lookup_data.h>
//#include <pargen/parsing_exception.h>


/**
 * .root
 * .title Pargen
 */

namespace Pargen {

using namespace M;

/*c
 * Position marker
 */
class ParserPositionMarker : public StReferenced
{
};

/*c
 * Parser control object
 */
class ParserControl : public StReferenced
{
public:
    virtual void setCreateElements (bool create_elements) = 0;

    virtual StRef<ParserPositionMarker> getPosition () = 0;

    virtual mt_throws Result setPosition (ParserPositionMarker *pmark) = 0;

    virtual void setVariant (ConstMemory variant_name) = 0;
};

// External users should not modify contents of ParserConfig objects.
class ParserConfig : public StReferenced
{
public:
    bool upwards_jumps;
};

StRef<ParserConfig> createParserConfig (bool upwards_jumps);

StRef<ParserConfig> createDefaultParserConfig ();

/*m*/
void optimizeGrammar (Grammar * mt_nonnull grammar);

/*m*/
//#warning TODO explicit error report
//#warning TODO handle return value
mt_throws Result parse (TokenStream    * mt_nonnull token_stream,
                        LookupData     *lookup_data,
                        void           *user_data,
                        Grammar        * mt_nonnull grammar,
                        ParserElement **ret_element,
                        StRef<StReferenced> *ret_element_container,
                        ConstMemory     default_variant = ConstMemory ("default"),
                        ParserConfig   *parser_config = NULL,
                        bool            debug_dump = false);

}


#endif /* PARGEN__PARSER_H__ */

