/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__CPP_VISITOR_H__
#define SCRUFFY__CPP_VISITOR_H__

#include <mycpp/mycpp.h>

namespace Scruffy {

using namespace MyCpp;

class CppVisitor : public virtual Object
{
public:
    virtual void declaration (Cpp::TypeDec *type_desc) {}

    virtual void beginNamespace () {}

    virtual void endNamespace () {}

    virtual void beginClass {}

    virtual void endClass {}

    virtual void beginFunction () {}

    virtual void endFunction () {}

    virtual void beginStatement () {}

    virtual void endStatement () {}

    virtual void expression () {}
};

}

#endif /* SCRUFFY__CPP_VISITOR_H__ */

