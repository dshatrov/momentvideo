/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>

#ifdef LIBMARY_GLIB
  #include <glib.h>
#else
  #include <cstdlib>
#endif

#include <libmary/libmary_thread_local.h>
#include <libmary/util_base.h>

#include <libmary/util_common.h>


namespace M {

void
randomSetSeed (Uint32 const seed)
{
  #ifdef LIBMARY_GLIB
    g_random_set_seed (seed);
  #else
    srand (seed);
  #endif
}

#warning FIXME true thread-safe rand on android
#ifdef LIBMARY_PLATFORM_ANDROID
static int rand_r (unsigned * const seed)
{
    ++(*seed);
    return (int) ((*seed) * 999983);
}
#endif

Uint32
randomUint32 ()
{
  #ifdef LIBMARY_GLIB
    /* From Glib docs on g_random_int():
     * "Return a random guint32 equally distributed over the range [0..2^32-1]." */
    Uint32 const res = (Uint32) g_random_int ();
    return res;
  #else
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
    if (!tlocal->rand_initialized) {
        tlocal->rand_state = (Uint32) (UintPtr) tlocal + (Uint32) (UintPtr) &tlocal;
        tlocal->rand_state += tlocal->time_microseconds;
        tlocal->rand_initialized = true;
    }

    #if (RAND_MAX) < 255
      #error RAND_MAX is too small
    #endif

    Uint32 val = 0;
    for (int i = 0; i < 4; ++i) {
        val <<= 8;
        int const r = rand_r (&tlocal->rand_state);
        val |= ((Uint32) r) & 0xff;
    }

    return val;
  #endif
}

}

