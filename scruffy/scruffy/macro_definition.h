/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__MACRO_DEFINITION__H__
#define SCRUFFY__MACRO_DEFINITION__H__


#include <libmary/libmary.h>


namespace Scruffy {

using namespace M;

class MacroDefinition : public StReferenced
{
public:
    StRef<String> name;
    StRef< List_< StRef<PpItem>, StReferenced > > replacement_list;
    StRef< List_< StRef<String>, StReferenced > > params;
    bool lparen;
};

}


#endif /* SCRUFFY__MACRO_DEFINITION__H__ */

