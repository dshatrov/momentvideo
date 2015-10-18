/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ASYNC_INPUT_STREAM__H__
#define LIBMARY__ASYNC_INPUT_STREAM__H__


#include <libmary/exception.h>
#include <libmary/cb.h>
#include <libmary/util_net.h>


namespace M {

class AsyncInputStream : public virtual Referenced
{
  #ifdef LIBMARY_WIN32_IOCP
    public:
      virtual mt_throws AsyncIoResult read (OVERLAPPED * mt_nonnull overlapped,
                                            Memory      mem,
                                            Size       *ret_nread) = 0;
  #else
    public:
      struct InputFrontend {
          void (*processInput) (void *cb_data);

          void (*processError) (Exception *exc_,
                                void      *cb_data);
      };

    protected:
      mt_const Cb<InputFrontend> input_frontend;

    public:
      virtual mt_throws AsyncIoResult read (Memory  mem,
                                            Size   *ret_nread) = 0;

      virtual mt_throws AsyncIoResult recvFrom (Memory      const mem,
                                                Size      * const ret_nread,
                                                IpAddress * const ret_addr)
      {
          if (ret_addr)
              *ret_addr = IpAddress();

          return read (mem, ret_nread);
      }

      mt_const void setInputFrontend (CbDesc<InputFrontend> const &input_frontend)
          { this->input_frontend = input_frontend; }
  #endif
};

}


#endif /* LIBMARY__ASYNC_INPUT_STREAM__H__ */

