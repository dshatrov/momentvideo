/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


/* This is a workaround for an ugly API bug in the GNU C library:
 * portable strerror_r() function requires _GNU_SOURCE macro not
 * to be defined, but g++ defines it unconditionally.
 * The consequences of undef'ing it before including
 * <string.h> are uncertain and may well be fatal.
 */
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#endif


/* See man strerror_r */
#define _XOPEN_SOURCE 600
#include <string.h>

#include <libmary/annotations.h>


#if 0
#ifdef LIBMARY_PLATFORM_WIN32
/* errno_t */ int strerror_s (char   *buf,
                              size_t  buflen,
                              int     errnum);
#endif
#endif


void libmary_library_lock ();
void libmary_library_unlock ();


// XSI-compliant strerror_r semantics.
int _libmary_strerror_r (int      const errnum,
			 char   * const mt_nonnull buf,
			 size_t   const buflen)
{
#ifdef LIBMARY_PLATFORM_WIN32
#if 0
    int const res = strerror_s (buf, buflen, errnum);
    if (res != 0)
      return -1;

    return 0;
#endif

    libmary_library_lock ();

    char const * const str = strerror (errnum);
    size_t len = strlen (str);
    if (len + 1 >= buflen)
        len = buflen - 1;

    memcpy (buf, str, len);
    buf [len] = 0;

    libmary_library_unlock ();
    return 0;
#else
    return strerror_r (errnum, buf, buflen);
#endif // LIBMARY_PLATFORM_WIN32
}

