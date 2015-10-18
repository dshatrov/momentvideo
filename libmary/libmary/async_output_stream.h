/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ASYNC_OUTPUT_STREAM__H__
#define LIBMARY__ASYNC_OUTPUT_STREAM__H__


#include <libmary/types.h>
#ifndef LIBMARY_PLATFORM_WIN32
  #include <sys/uio.h>
#endif

#include <libmary/exception.h>
#include <libmary/cb.h>
#include <libmary/util_net.h>


namespace M {

class AsyncOutputStream : public virtual Referenced
{
  #ifdef LIBMARY_WIN32_IOCP
    public:
      virtual mt_throws AsyncIoResult write (OVERLAPPED  * mt_nonnull overlapped,
                                             ConstMemory  mem,
                                             Size        *ret_nwritten) = 0;

      virtual mt_throws AsyncIoResult writev (OVERLAPPED  * mt_nonnull overlapped,
                                              WSABUF      * mt_nonnull buffers,
                                              Count        buffer_count,
                                              Size        *ret_nwritten) = 0;
  #else
    public:
      struct OutputFrontend {
          void (*processOutput) (void *cb_data);
      };

    protected:
      mt_const Cb<OutputFrontend> output_frontend;

    public:
      virtual mt_throws AsyncIoResult write (ConstMemory  mem,
                                             Size        *ret_nwritten) = 0;

      virtual mt_throws AsyncIoResult writev (struct iovec *iovs,
                                              Count         num_iovs,
                                              Size         *ret_nwritten);

      mt_const void setOutputFrontend (CbDesc<OutputFrontend> const &output_frontend)
          { this->output_frontend = output_frontend; }
  #endif

      virtual AsyncIoResult sendVectorTo (IpAddress     addr,
                                        #ifdef LIBMARY_WIN32_IOCP
                                          OVERLAPPED   * mt_nonnull overlapped,
                                          WSABUF       * mt_nonnull buffers,
                                          Count         buffer_count,
                                        #else
                                          struct iovec * mt_nonnull iovs,
                                          Count         num_iovs,
                                        #endif
                                          Size         *ret_nwritten)
      {
          (void) addr;
        #ifdef LIBMARY_WIN32_IOCP
          (void) overlapped; (void) buffers; (void) buffer_count;
        #else
          (void) iovs; (void) num_iovs;
        #endif
          (void) ret_nwritten;
          unreachable ();
          return AsyncIoResult::Error;
      }

      virtual Result seekBeg (Uint64 const /* offset */) { return Result::Failure; }
};

}


#endif /* LIBMARY__ASYNC_OUTPUT_STREAM__H__ */

