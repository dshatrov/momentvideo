/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__COMMON_SOCKET__WIN32_IOCP__H__
#define LIBMARY__COMMON_SOCKET__WIN32_IOCP__H__


#include <libmary/connection.h>
#include <libmary/poll_group.h>


namespace M {

// TODO Use CommonSocket in TcpConnection to reduce code duplication.
class CommonSocket : public Connection
{
protected:
    mt_const SOCKET socket_fd;

public:
  mt_iface (Connection)
    mt_iface (AsyncInputStream)
      mt_throws AsyncIoResult read (OVERLAPPED * mt_nonnull overlapped,
                                    Memory      mem,
                                    Size       *ret_nread);
    mt_iface_end

    mt_iface (AsyncOutputStream)
      mt_throws AsyncIoResult write (OVERLAPPED  * mt_nonnull overlapped,
                                     ConstMemory  mem,
                                     Size        *ret_nwritten);

      mt_throws AsyncIoResult writev (OVERLAPPED * mt_nonnull overlapped,
                                      WSABUF     * mt_nonnull buffers,
                                      Count       buffer_count,
                                      Size       *ret_nwritten);
    mt_iface_end
  mt_iface_end

     CommonSocket ();
    ~CommonSocket ();
};


}


#endif /* LIBMARY__COMMON_SOCKET__WIN32_IOCP__H__ */

