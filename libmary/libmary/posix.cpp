/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <libmary/exception.h>
#include <libmary/io.h>
#include <libmary/android_logcat_output_stream.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/native_file.linux.h>
#else
  #include <libmary/native_file.h>
#endif
#include <libmary/log.h>


namespace M {

void libMary_platformInit ()
{
    {
      // Blocking SIGPIPE for write()/writev().
        struct sigaction act;
        memset (&act, 0, sizeof (act));
        act.sa_handler = SIG_IGN;
        // TODO -1 on error
        sigemptyset (&act.sa_mask);
        // TODO -1 on error
        sigaddset (&act.sa_mask, SIGPIPE);
        if (sigaction (SIGPIPE, &act, NULL) == -1)
            fprintf (stderr, "sigaction() failed: %s", errnoString (errno));
    }

    // Calling tzset() for localtime_r() to behave correctly.
    tzset ();

    {
      // Allocating on heap to avoid problems with deinitialization order
      // of static data.

      #ifdef LIBMARY_PLATFORM_ANDROID
        outs = new (std::nothrow) AndroidLogcatOutputStream ();
      #else
        outs = new (std::nothrow) NativeFile (NULL /* embed_container */, STDOUT_FILENO);
      #endif
        assert (outs);

      #ifdef LIBMARY_PLATFORM_ANDROID
        errs = new (std::nothrow) AndroidLogcatOutputStream ();
      #else
        errs = new (std::nothrow) NativeFile (NULL /* embed_container */, STDERR_FILENO);
      #endif
        assert (errs);

        setLogStream (errs,
                      NULL /* new_logs_release_cb */,
                      NULL /* new_logs_release_cb_data */,
                      true /* add_buffered_stream */);
    }

// TODO What's this? - this is probably to avoid forked zombies.
#if 0
    {
	struct sigaction sa;
	zeroMemory (&sa, sizeof sa);
	sa.sa_handler = sigchld_handler;
	sigemptyset (&sa.sa_mask);
/* I'd like to have a non-restarting signal
 * to test MyNC libraries for correct handling of EINTR.
	sa.sa_flags = SA_RESTART;
 */
	// TODO sigaddset?
	sa.sa_flags = 0;
	if (sigaction (SIGCHLD, &sa, NULL) == -1) {
	    printError ("sigthread_func: (fatal) sigaction");
	    _exit (-1);
	}
    }
#endif
}

}

