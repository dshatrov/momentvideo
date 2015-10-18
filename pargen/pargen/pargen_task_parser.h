/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__PARGEN_TASK_PARSER__H__
#define PARGEN__PARGEN_TASK_PARSER__H__


#include <libmary/libmary.h>

#include <pargen/token_stream.h>

#include <pargen/declarations.h>
#include <pargen/parsing_exception.h>


namespace Pargen {

using namespace M;

class PargenTask : public StReferenced
{
public:
    List< StRef<Declaration> > decls;
};

mt_throws Result parsePargenTask (TokenStream       * mt_nonnull token_stream,
                                  StRef<PargenTask> * mt_nonnull ret_pargen_task);

void dumpDeclarations (PargenTask const * mt_nonnull pargen_task);

}


#endif /* PARGEN__PARGEN_TASK_PARSER__H__ */

