/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__UTIL__H__
#define PARGEN__UTIL__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

StRef<String> capitalizeName (ConstMemory name,
                              bool        keep_underscore = true);

StRef<String> capitalizeNameAllCaps (ConstMemory name);

StRef<String> lowercaseName (ConstMemory name);

}


#endif /* PARGEN__UTIL__H__ */

