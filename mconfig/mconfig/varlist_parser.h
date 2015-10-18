/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents,
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MCONFIG__VARLIST_PARSER__H__
#define MCONFIG__VARLIST_PARSER__H__


#include <libmary/types.h>
#include <pargen/parser.h>

#include <mconfig/varlist.h>


namespace MConfig {

using namespace M;

class VarlistParser
{
private:
    mt_const StRef<Pargen::Grammar> grammar;
    mt_const StRef<Pargen::ParserConfig> parser_config;

public:
    mt_throws Result parseVarlist (ConstMemory  filename,
                                   Varlist     *varlist);

    VarlistParser ();
};

}


#endif /* MCONFIG__VARLIST_PARSER__H__ */

