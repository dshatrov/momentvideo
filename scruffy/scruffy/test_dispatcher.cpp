/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <mycpp/io.h>

#include <scruffy/test_dispatcher.h>

using namespace MyCpp;

namespace Scruffy {

void
TestDispatcher::declaration (Cpp::Identifier *identifier)
{
    errf->print ("Scruffy.TestDispatcher.declaration");
}

void
TestDispatcher::definition (Cpp::Identifier *identifier)
{
    errf->print ("Scruffy.TestDispatcher.definition");
}

}

