/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MODULE__H__
#define LIBMARY__MODULE__H__


#include <libmary/types.h>


namespace M {

mt_throws Result loadModule (ConstMemory  filename,
                             void        *app_specific);

}


#endif /* LIBMARY__MODULE__H__ */

