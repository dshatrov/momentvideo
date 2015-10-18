/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cstdio>

#include <libmary/util_base.h>

#include <libmary/referenced.h>


namespace M {

#ifdef LIBMARY_REF_TRACING
void
Referenced::traceRef ()
{
    char * const bt = rawCollectBacktrace ();
    fprintf (stderr, "reftrace:   ref 0x%lx, rc %u\n%s\n",
	     (unsigned long) this, (unsigned) refcnt, bt ? bt : "");
    delete[] bt;
}

void
Referenced::traceUnref ()
{
    char * const bt = rawCollectBacktrace ();
    fprintf (stderr, "reftrace: unref 0x%lx, rc %u\n%s\n",
	     (unsigned long) this, (unsigned) refcnt, bt ? bt : "");
    delete[] bt;
}
#endif // LIBMARY_REF_TRACING

}

