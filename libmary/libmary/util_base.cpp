/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>

#if defined __linux__ && !defined LIBMARY_PLATFORM_ANDROID
  #include <execinfo.h> /* for backtraces */
#endif

#include <cstdlib>
#include <cstdio>

#include <libmary/log.h>

#include <libmary/util_base.h>


extern "C" {

void libmary_library_lock ()
{
    M::libraryLock ();
}

void libmary_library_unlock ()
{
    M::libraryUnlock ();
}

}


namespace M {

Mutex _libMary_library_mutex;
Mutex _libMary_helper_mutex;

void _libMary_unreachable_ (ConstMemory const &mem,
                            ConstMemory const &func)
{
    fprintf (stderr, "%.*s%.*s: unreachable point reached\n", (int) mem.len(), mem.mem(), (int) func.len(), func.mem());
    abort ();
}

char*
rawCollectBacktrace ()
{
#if defined __linux__ && !defined LIBMARY_PLATFORM_ANDROID
    char const * const fail_str = "[Failed to get the backtrace]";

    void *addrs [128];
    int const naddrs = backtrace (addrs, 128);
    if (naddrs < 0 ||
	naddrs > 128)
    {
	logW_ (_func, "unexpected return value from backtrace()");

	Size const fail_len = strlen (fail_str);
	char * const bt = new char [fail_len + 1];
	memcpy (bt, fail_str, fail_len + 1);
	return bt;
    }

    if (naddrs == 128)
	logW_ (_func, "the backtrace is possibly truncated, 128 entries max");

    char ** const bt_arr = backtrace_symbols (addrs, naddrs);
    if (bt_arr == NULL) {
	logW_ (_func,  "backtrace_symbols() failed");

	Size const fail_len = strlen (fail_str);
	char * const bt = new char [fail_len + 1];
	memcpy (bt, fail_str, fail_len + 1);
	return bt;
    }

    unsigned long i;
    unsigned long total_len = 0;

    for (i = 0; i < (unsigned long) naddrs; i++) {
	if (bt_arr [i] == NULL)
	    continue;

	unsigned long const str_len = strlen (bt_arr [i]);
	if (total_len + str_len <= total_len)
	    return NULL;
	total_len += str_len;
	/* Plus one byte for a newline. */
	if (total_len + 1 <= total_len)
	    return NULL;
	total_len += 1;
    }

    if (total_len > 0) {
	/* Last newline is unnecessary. */
	total_len --;
    }

    if (total_len == 0)
	return NULL;

    char * const backtrace = new char [total_len + 1];

    unsigned long pos = 0;
    for (i = 0; i < (unsigned long) naddrs; i++) {
	if (bt_arr [i] == NULL)
	    continue;

	unsigned long const str_len = strlen (bt_arr [i]);
	assert (str_len <= total_len - pos);
	memcpy (backtrace + pos, bt_arr [i], str_len);
	pos += str_len;
	/* String.allocate() allocates one additional byte
	 * for the trailing zero. The last newline will
	 * go exactly to that byte. */
	backtrace [pos] = '\n';
	pos ++;
    }
    backtrace [total_len] = 0;

    free (bt_arr);

    return backtrace;
#else
    return NULL;
#endif
}

}

