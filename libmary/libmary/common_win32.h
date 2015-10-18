/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__COMMON_WIN32__H__
#define LIBMARY__COMMON_WIN32__H__


#include <libmary/util_net.h>


namespace M {

mt_throws Result _libMary_win32_socket_tcp (SOCKET * mt_nonnull ret_socket_fd,
                                            bool    overlapped);

mt_throws Result _libMary_win32_setsockopt_nodelay (SOCKET socket_fd);

mt_throws Result _libMary_win32_setsockopt_reuseaddr (SOCKET socket_fd);

mt_throws Result _libMary_win32_bind (SOCKET    socket_fd,
                                      IpAddress addr);

mt_throws Result _libMary_win32_listen (SOCKET socket_fd);

mt_throws Result _libMary_win32_closesocket (SOCKET socket_fd);

}


#endif /* LIBMARY__COMMON_WIN32__H__ */

