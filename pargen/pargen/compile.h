/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__COMPILE__H__
#define PARGEN__COMPILE__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

class CompilationOptions : public StReferenced
{
public:
    StRef<String> module_name;
    StRef<String> capital_module_name;
    StRef<String> all_caps_module_name;

    StRef<String> capital_namespace_name;

    StRef<String> header_name;
    StRef<String> capital_header_name;
    StRef<String> all_caps_header_name;
};

}


#endif /* PARGEN__COMPILE__H__ */

