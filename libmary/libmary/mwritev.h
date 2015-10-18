/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MWRITEV__H__
#define LIBMARY__MWRITEV__H__


#include <libmary/types.h>

#include <sys/uio.h>


namespace M {

bool libMary_mwritevAvailable ();

mt_throws Result libMary_mwritev (int  num_fds,
				  int *fds,
				  struct iovec **iovs,
				  int *num_iovs,
				  int *ret_res);

mt_throws Result libMary_mwritevSingle (int  fd,
					struct iovec *iovs,
					int  num_iovs,
					int *ret_res);

mt_const void libMary_mwritevInit ();

}


#endif /* LIBMARY__MWRITEV__H__ */

