/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>

#ifdef LIBMARY_GLIB
  #include <gmodule.h>
#else
  #include <dlfcn.h>
#endif

#include <libmary/util_base.h>
#include <libmary/log.h>

#include <libmary/module.h>


namespace M {

typedef bool (*ModuleInitFunc) (void *app_specific);

mt_throws Result
loadModule (ConstMemory   const filename,
            void        * const app_specific)
{
  #ifdef LIBMARY_GLIB
    GModule * const module = g_module_open ((gchar const*) makeString (filename)->cstr(),
                                            (GModuleFlags) (G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL));
    if (!module) {
        exc_throw (InternalException, InternalException::BackendError);
        logE_ (_func, "failed to open module ", filename, ": ",  g_module_error());
        return Result::Failure;
    }

    ModuleInitFunc init_ptr;
    if (!g_module_symbol (module, "libMary_moduleInit_wrapper", (gpointer*) &init_ptr)) {
        exc_throw (InternalException, InternalException::BackendError);
        logE_ (_func, "failed to open module ", filename, ": "
               "g_module_symbol (\"libMary_moduleInit_wrapper\") failed: ", g_module_error());
        return Result::Failure;
    }

    if (!init_ptr (app_specific)) {
        logE_ (_func, "could notd initialize module ", filename);
        return Result::Failure;
    }

    return Result::Success;
  #else
    libraryLock ();
    void * const handle = dlopen (
          #ifdef LIBMARY_PLATFORM_MACOSX
            makeString (filename, ".0.dylib")->cstr(),
          #else
            makeString (filename, ".so")->cstr(),
          #endif
            RTLD_LAZY);
    if (!handle) {
        char const * const err_str = dlerror ();
        libraryUnlock ();
        exc_throw (InternalException, InternalException::BackendError);
        logE_ (_func, "dlopen (", filename, ") failed: ", err_str);
        return Result::Failure;
    }

    dlerror (); // Clearing any old error conditions. See man dlsym(3).
    void * const init_ptr = dlsym (handle, "libMary_moduleInit");
    if (!init_ptr) {
        char const * const err_str = dlerror ();
        libraryUnlock ();
        exc_throw (InternalException, InternalException::BackendError);
        logE_ (_func, "dlsym (", filename, ", libMary_moduleInit) failed: ", err_str);
        return Result::Failure;
    }
    libraryUnlock ();

    bool const res = ((ModuleInitFunc) init_ptr) (app_specific);
    if (!res) {
        exc_throw (InternalException, InternalException::BackendError);
        logE_ (_func, "module init failed: ", filename);
    }

    return res ? Result::Success : Result::Failure;
  #endif
}

}

