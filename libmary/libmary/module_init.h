/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MODULE_INIT__H__
#define LIBMARY__MODULE_INIT__H__


#include <libmary/types.h>

#ifdef LIBMARY_GLIB
  #include <gmodule.h>
#endif


// This file must be included just once by module code.
// It provides LibMary-specific static initialization functions.
// Glib's implementation of modules is used currently.


extern "C" {

extern bool libMary_moduleInit (void * const app_specific);

#ifdef LIBMARY_GLIB
G_MODULE_EXPORT bool libMary_moduleInit_wrapper (void * const app_specific)
        { return libMary_moduleInit (app_specific); }
#endif

}


#endif /* LIBMARY__MODULE_INIT__H__ */

