/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_BASE__H__
#define LIBMARY__UTIL_BASE__H__


#include <cstdlib>


#include <libmary/types_base.h>

// Version of assert() which is never optimized out.
#define assert_hard(a) do { bool const res = (a); assert (res && #a); if (mt_unlikely (!res)) abort (); } while (0)

#include <libmary/memory.h>
#include <libmary/mutex.h>


extern "C" {
    void libmary_library_lock ();
    void libmary_library_unlock ();
}


namespace M {

/* 07.05.26
 * I introduced libraryLock and libraryUnlock to wrap
 * non-reentrant getservbyname library call.
 * It looks like Linux does not support reentrant
 * getservbyname_r call. */

/* 08.10.28
 * One should use libraryLock() in every case of using a single-threaded
 * library. Remember that such libraries could call non-reentrant glibc
 * functions, which means that they should all be synchronized with each
 * other. libraryLock/Unlock() is the synchronization mechanism to use
 * in case of using MyCpp. */

extern Mutex _libMary_library_mutex;
extern Mutex _libMary_helper_mutex;

/*m Aquires the lock that is used to protect non-reentrant library calls
 * (mainly for glibc calls). */
static inline void libraryLock ()
{
    _libMary_library_mutex.lock ();
}

/*m Releases the lock that is used to protect non-reentrant library calls
 * (mainly for glibc calls). */
static inline void libraryUnlock ()
{
    _libMary_library_mutex.unlock ();
}

static inline void helperLock ()
{
    _libMary_helper_mutex.lock ();
}

static inline void helperUnlock ()
{
    _libMary_helper_mutex.unlock ();
}

char* rawCollectBacktrace ();

}


#endif /* LIBMARY__UTIL_BASE__H__ */

