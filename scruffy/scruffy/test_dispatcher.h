/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__TEST_DISPATCHER__H__
#define SCRUFFY__TEST_DISPATCHER__H__

#include <mycpp/mycpp.h>

#include <scruffy/cpp_dispatcher.h>

namespace Scruffy {

using namespace MyCpp;

class TestDispatcher : public CppDispatcher
{
public:
#if 0
    void declaration (Cpp::Identifier *identifier);

    void definition (Cpp::Identifier *identifier);
#endif
};

}

#endif /* SCRUFFY__TEST_DISPATCHER__H__ */

