/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cstdio>

#include <libmary/io.h>
#include <libmary/native_file.h>
#include <libmary/log.h>


namespace M {

void libMary_platformInit ()
{
    _tzset ();

    {
	// NOTE: We never call WSACleanup(). This is probably wrong.
	WORD wsa_version = MAKEWORD (2, 2);
	WSADATA wsa_data;
	if (WSAStartup (wsa_version, &wsa_data)) {
	    fprintf (stderr, "FATAL: WSAStartup() failed\n");
	    fflush (stderr);
            abort ();
	}
    }

    {
      // TODO Should not close these handles. Can this be ensured by some
      //      way other than a deliberate memory leak?
      //      The same applies to posix.cpp

        outs = new (std::nothrow) NativeFile (NULL /* embed_container */, GetStdHandle (STD_OUTPUT_HANDLE));
        assert (outs);

        errs = new (std::nothrow) NativeFile (NULL /* embed_container */, GetStdHandle (STD_ERROR_HANDLE));
        assert (errs);

        setLogStream (errs,
                      NULL /* new_logs_release_cb */,
                      NULL /* new_logs_release_cb_data */,
                      true /* add_buffered_stream */);
    }
}

}

