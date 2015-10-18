/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__COMMON_SOCKET__POSIX__H__
#define LIBMARY__COMMON_SOCKET__POSIX__H__


#include <libmary/connection.h>
#include <libmary/poll_group.h>


namespace M {

// TODO Use CommonSocket in TcpConnection to reduce code duplication.
class CommonSocket : public Connection
{
protected:
    mt_const bool auto_normal_again;

    mt_const int fd;

    Cb<PollGroup::Feedback> feedback;

    // Synchronized by processEvents() and also used by read(). This implies
    // that read() must be called from the same thread as processEvents(),
    // which is not very pleasant.
    bool hup_received;

    void requestInput ()
    {
	if (feedback && feedback->requestInput)
	    feedback.call (feedback->requestInput);
    }

    void requestOutput ()
    {
	if (feedback && feedback->requestOutput)
	    feedback.call (feedback->requestOutput);
    }

public:
  mt_iface (Connection)
    mt_iface (AsyncInputStream)
      mt_throws AsyncIoResult read (Memory  mem,
                                    Size   *ret_nread);
    mt_iface_end

    mt_iface (AsyncOutputStream)
      mt_throws AsyncIoResult write (ConstMemory  mem,
                                     Size        *ret_nwritten);

      mt_throws AsyncIoResult writev (struct iovec *iovs,
                                      Count         num_iovs,
                                      Size         *ret_nwritten);
    mt_iface_end

#if 0
    // Note that close() closes the file descriptor, which may cause races
    // if the connection object is still in use, i.e. it is referenced and
    // read/write methods may potentially be called.
    mt_throws Result close ();
#endif

#ifdef LIBMARY_ENABLE_MWRITEV
    int getFd () { return fd; }
#endif
  mt_iface_end

    CommonSocket (EmbedContainer *embed_container,
                  bool            auto_normal_again);

    ~CommonSocket ();
};

}


#endif /* LIBMARY__COMMON_SOCKET__POSIX__H__ */

