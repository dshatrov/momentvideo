/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__WIN32__H__
#define LIBMARY__WIN32__H__


#include <libmary/types.h>
//#include <mswsock.h>


namespace M {

#if 0
#ifdef LIBMARY_WIN32_IOCP
extern mt_const LPFN_CONNECTEX _libMary_ConnectEx;
#endif
#endif

void libMary_win32Init ();

}


#endif /* LIBMARY__WIN32__H__ */

