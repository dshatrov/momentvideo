/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cstdlib>
#include <cstdio>
#include <locale.h>


#include <libmary/libmary.h>

#include <libmary/libmary_thread_local.h>

#ifdef LIBMARY_ENABLE_MWRITEV
#include <libmary/mwritev.h>
#endif


namespace M {

void libMary_platformInit ();

#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY_GLIB
    volatile gint _libMary_dummy_mb_int = 0;
  #endif
#endif

OutputStream *outs;
OutputStream *errs;
OutputStream *logs;

Stat *_libMary_stat;

void libMaryInit ()
{
    {
	static bool initialized = false;

	if (initialized) {
	    return;
	}
	initialized = true;
    }

    // Setting numeric locale for snprintf() to behave uniformly in all cases.
    // Specifically, we need dot ('.') to be used as a decimal separator.
    if (setlocale (LC_NUMERIC, "C") == NULL)
        fprintf (stderr, "WARNING: Could not set LC_NUMERIC locale to \"C\"\n");

  #ifndef LIBMARY_PLATFORM_WIN32
    // GStreamer calls setlocale(LC_ALL, ""), which is lame. We fight this with setenv().
    if (setenv ("LC_NUMERIC", "C", 1 /* overwrite */) == -1)
        perror ("WARNING: Could not set LC_NUMERIC environment variable to \"C\"");
  #endif

  #ifdef LIBMARY_MT_SAFE
    #ifdef LIBMARY__OLD_GTHREAD_API
      if (!g_thread_get_initialized ())
          g_thread_init (NULL);
    #endif
  #endif

    initStatCounters ();

    _libMary_stat = new (std::nothrow) Stat;
    assert (_libMary_stat);

    libMary_threadLocalInit ();
    libMary_platformInit ();

  // log*() logging is now available.

    if (!updateTime ())
        logE_ (_func, exc->toString());

  #ifdef LIBMARY_ENABLE_MWRITEV
    libMary_mwritevInit ();
  #endif

    randomSetSeed ((Uint32) getTimeMicroseconds());
}

void libMaryRelease ()
{
    releaseStatCounters ();
  // Release thread-local data here?
  // This could be done after careful deinitialization is implemented.
    libMary_releaseThreadLocalForMainThread ();
}

}

