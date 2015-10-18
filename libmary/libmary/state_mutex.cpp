/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


//#include <cstdio>

#include <libmary/types.h>
#include <libmary/libmary_thread_local.h>

#include <libmary/state_mutex.h>


namespace M {

#ifdef LIBMARY_MT_SAFE
void
StateMutex::lock ()
{
#ifdef LIBMARY_MT_SAFE
    mutex.lock ();
#endif

    {
	LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

//	fprintf (stderr, "0x%lx %s: tlocal 0x%lx, BEFORE %lu\n", (unsigned long) this,
//		 __func__, (unsigned long) tlocal, (unsigned long) tlocal->state_mutex_counter);
	++ tlocal->state_mutex_counter;
//	fprintf (stderr, "0x%lx %s: tlocal 0x%lx, AFTER  %lu\n", (unsigned long) this,
//		 __func__, (unsigned long) tlocal, (unsigned long) tlocal->state_mutex_counter);
    }
}

void StateMutex::unlock ()
{
    {
	LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

//	fprintf (stderr, "0x%lx %s: tlocal 0x%lx, %lu\n", (unsigned long) this,
//		 __func__, (unsigned long) tlocal, (unsigned long) tlocal->state_mutex_counter);
	assert (tlocal->state_mutex_counter > 0);
	-- tlocal->state_mutex_counter;

	if (tlocal->state_mutex_counter == 0) {
#ifdef LIBMARY_MT_SAFE
	    mutex.unlock ();
#endif

	    deletionQueue_process ();
	    return;
	}
    }

#ifdef LIBMARY_MT_SAFE
    mutex.unlock ();
#endif
}
#endif

}

