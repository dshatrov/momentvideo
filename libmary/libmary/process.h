/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__PROCESS__H__
#define LIBMARY__PROCESS__H__


#include <libmary/types.h>
#include <libmary/poll_group.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/native_file.linux.h>
  #include <libmary/native_async_file.linux.h>
#else
  #include <libmary/native_file.h>
  #include <libmary/native_async_file.h>
#endif


namespace M {

Result initProcessSpawner ();
Result initProcessSpawner_addExitPoll (PollGroup * mt_nonnull poll_group);

extern ConstMemory spawnProcess_fd;

typedef Result SpawnProcessType (ConstMemory        path,
                                 ConstMemory const *args,
                                 Count              num_args,
                               #ifdef LIBMARY_PLATFORM_WIN32
                                 HANDLE*
                               #else
                                 int*
                               #endif
                                                    fds,
                                 Count              num_fds);
SpawnProcessType spawnProcess;

class ProcessInputPipe;
class ProcessOutputPipe;
class ProcessAsyncOutputPipe;
class ProcessAsyncInputPipe;

typedef Result SpawnProcessWithPipesType (ConstMemory              path,
                                          ConstMemory             *args,
                                          Count                    num_args,
                                          Count                    num_input_pipes,
                                          ProcessInputPipe       **ret_input_pipes,
                                          Count                    num_output_pipes,
                                          ProcessOutputPipe      **ret_output_pipes,
                                          Count                    num_async_input_pipes,
                                          ProcessAsyncInputPipe  **ret_async_input_pipes,
                                          Count                    num_async_output_pipes,
                                          ProcessAsyncOutputPipe **ret_async_output_pipes);
SpawnProcessWithPipesType spawnProcessWithPipes;

#ifndef LIBMARY_PLATFORM_WIN32
class ProcessInputPipe : public InputStream
{
    friend SpawnProcessType spawnProcess;
    friend SpawnProcessWithPipesType spawnProcessWithPipes;

  private:
    NativeFile file;

    mt_const void init (
                      #ifdef LIBMARY_PLATFORM_WIN32
                        HANDLE const fd
                      #else
                        int const fd
                      #endif
                        )
    {
        file.setFd (fd);
    }

  public:
    // TODO getAsyncInputStream(), getAsyncOutputStream()
    //      For Linux, based on named pipes.
    //      For Windows, based on local TCP connections.

    mt_iface (InputStream)
      mt_throws IoResult read (Memory   const mem,
                               Size   * const ret_nread)
          { return file.read (mem, ret_nread); }
    mt_iface_end

    ProcessInputPipe ()
        : file (NULL /* embed_container */)
    {}
};

class ProcessOutputPipe : public OutputStream
{
    friend SpawnProcessType spawnProcess;
    friend SpawnProcessWithPipesType spawnProcessWithPipes;

  private:
    NativeFile file;

    mt_const void init (
                      #ifdef LIBMARY_PLATFORM_WIN32
                        HANDLE const fd
                      #else
                        int const fd
                      #endif
                        )
    {
        file.setFd (fd);
    }

  public:
    mt_iface (OutputStream)
      mt_throws Result write (ConstMemory   const mem,
                              Size        * const ret_nwritten)
          { return file.write (mem, ret_nwritten); }

      mt_throws Result writev (struct iovec * const iovs,
                               Count          const num_iovs,
                               Size         * const ret_nwritten)
          { return file.writev (iovs, num_iovs, ret_nwritten); }

      mt_throws Result flush ()
          { return file.flush (); }
    mt_iface_end

    ProcessOutputPipe ()
        : file (NULL /* embed_container */)
    {}
};

class ProcessAsyncInputPipe : public Object,
                              public AsyncInputStream
{
    friend SpawnProcessType spawnProcess;
    friend SpawnProcessWithPipesType spawnProcessWithPipes;

  private:
    Embed<NativeAsyncFile> file;

    #ifdef LIBMARY_PLATFORM_WIN32
      mt_const void init (HANDLE const fd) { file->setFd (fd); }
    #else
      mt_const void init (int const fd) { file->setFd (fd); }
    #endif

  public:
    mt_iface (AsyncInputStream)
      #ifdef LIBMARY_WIN32_IOCP
        mt_throws AsyncIoResult read (OVERLAPPED * const mt_nonnull overlapped,
                                      Memory       const mem,
                                      Size       * const ret_nread)
            { return file->read (overlapped, mem, ret_nread); }
      #else
        mt_throws AsyncIoResult read (Memory   const mem,
                                      Size   * const ret_nread)
            { return file->read (mem, ret_nread); }
      #endif
    mt_iface_end

    int getFd () { return file->getFd(); }

    ProcessAsyncInputPipe (EmbedContainer * const embed_container)
        : Object (embed_container),
          file   (this /* embed_container */)
    {}
};

class ProcessAsyncOutputPipe : public Object,
                               public AsyncOutputStream
{
    friend SpawnProcessType spawnProcess;
    friend SpawnProcessWithPipesType spawnProcessWithPipes;

  private:
    Embed<NativeAsyncFile> file;

    #ifdef LIBMARY_PLATFORM_WIN32
      mt_const void init (HANDLE const fd) { file->setFd (fd); }
    #else
      mt_const void init (int const fd) { file->setFd (fd); }
    #endif

  public:
    mt_iface (AsyncOutputStream)
      #ifdef LIBMARY_WIN32_IOCP
        mt_throws AsyncIoResult write (OVERLAPPED  * const mt_nonnull overlapped,
                                       ConstMemory   const mem,
                                       Size        * const ret_nwritten)
            { return file->write (overlapped, mem, ret_nwritten); }

        mt_throws AsyncIoResult writev (OVERLAPPED  * const mt_nonnull overlapped,
                                        WSABUF      * const mt_nonnull buffers,
                                        Count         const buffer_count,
                                        Size        * const ret_nwritten);
            { return file->writev (overlapped, buffers, buffer_count, ret_nwritten); }
      #else
        mt_throws AsyncIoResult write (ConstMemory   const mem,
                                       Size        * const ret_nwritten)
            { return file->write (mem, ret_nwritten); }

        mt_throws AsyncIoResult writev (struct iovec * const iovs,
                                        Count          const num_iovs,
                                        Size         * const ret_nwritten)
            { return file->writev (iovs, num_iovs, ret_nwritten); }
      #endif
    mt_iface_end

    int getFd () { return file->getFd(); }

    ProcessAsyncOutputPipe (EmbedContainer * const embed_container)
        : Object (embed_container),
          file   (this /* embed_container */)
    {}
};
#endif

}


#endif /* LIBMARY__PROCESS__H__ */

