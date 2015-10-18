/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__SOURCE_COMPILER__H__
#define PARGEN__SOURCE_COMPILER__H__


#include <libmary/libmary.h>

#include <pargen/declarations.h>
#include <pargen/pargen_task_parser.h>
#include <pargen/compile.h>


namespace Pargen {

using namespace M;

mt_throws Result compileSource (File                     *file,
                                PargenTask const         *pargen_task,
                                CompilationOptions const *opts);

}


#endif /* PARGEN__SOURCE_COMPILER__H__ */

