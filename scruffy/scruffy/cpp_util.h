/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__CPP_UTIL_H__
#define SCRUFFY__CPP_UTIL_H__

#include <scruffy/cpp.h>

namespace Scruffy {

using namespace MyCpp;

namespace Cpp {

void dumpContainerInfo (Container const &container);

void dumpMemberInfo (Member const *member /* non-null */);

}

}

#endif /* SCRUFFY__CPP_UTIL_H__ */

