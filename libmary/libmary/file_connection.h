/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__FILE_CONNECTION__H__
#define LIBMARY__FILE_CONNECTION__H__


#include <libmary/types.h>
#include <libmary/connection.h>
#include <libmary/file.h>
#ifdef LIBMARY_WIN32_IOCP
  #include <libmary/iocp_poll_group.h>
#endif


namespace M {

class FileConnection : public Connection
{
  private:
    #ifdef LIBMARY_WIN32_IOCP
      StateMutex mutex;

      DeferredProcessor::Task output_complete_task;
      DeferredProcessor::Registration deferred_reg;

      mt_mutex (mutex) Overlapped *overlapped;
      mt_mutex (mutex) Size        bytes_transferred;
      mt_mutex (mutex) bool        output_completion_pending;
      mt_mutex (mutex) Cb<Overlapped::IoCompleteCallback> io_complete_cb;

      static bool outputCompleteTask (void *_self);
    #endif

    mt_const Ref<File> file;

  public:
    mt_iface (Connection)
      mt_iface (AsyncInputStream)
        mt_throws AsyncIoResult read (
                                    #ifdef LIBMARY_WIN32_IOCP
                                      OVERLAPPED  * mt_nonnull overlapped,
                                    #endif
                                      Memory       mem,
                                      Size        *ret_nread);
      mt_iface_end

      mt_iface (AsyncOutputStream)
        mt_throws AsyncIoResult write (
                                     #ifdef LIBMARY_WIN32_IOCP
                                       OVERLAPPED  * mt_nonnull overlapped,
                                     #endif
                                       ConstMemory  mem,
                                       Size        *ret_nwritten);

        mt_throws AsyncIoResult writev (
                                      #ifdef LIBMARY_WIN32_IOCP
                                        OVERLAPPED   * mt_nonnull overlapped,
                                        WSABUF       * mt_nonnull buffers,
                                      #else
                                        struct iovec *iovs,
                                      #endif
                                        Count         num_iovs,
                                        Size         *ret_nwrittev);

        Result seekBeg (Uint64 offset);
      mt_iface_end

      #ifdef LIBMARY_ENABLE_MWRITEV
        int getFd () { return file->getFd(); }
      #endif
    mt_iface_end

    mt_const void init (DeferredProcessor * const deferred_processor,
                        File              * const mt_nonnull file)
    {
        this->file = file;
      #ifdef LIBMARY_WIN32_IOCP
        deferred_reg.setDeferredProcessor (deferred_processor);
      #else
        (void) deferred_processor;
      #endif
    }

    FileConnection (EmbedContainer * const embed_container)
        : Connection (embed_container)
        #ifdef LIBMARY_WIN32_IOCP
          ,
          overlapped (NULL),
          bytes_transferred (0),
          output_completion_pending (false)
        #endif
    {
      #ifdef LIBMARY_WIN32_IOCP
        output_complete_task.cb =
                CbDesc<DeferredProcessor::TaskCallback> (outputCompleteTask, this, this);
      #endif
    }
};

}


#endif /* LIBMARY__FILE_CONNECTION__H__ */

