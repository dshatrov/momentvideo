/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <libmary/util_posix.h>
#include <libmary/util_dev.h>
#include <libmary/log.h>

#include <libmary/process.h>


namespace M {

static int glob_s_unix = -1;

static unsigned char const spawnProcess_fd_ [1] = { 0 };
ConstMemory spawnProcess_fd (spawnProcess_fd_, 0);

static Result doSpawnProcess (char const  * const path,
                              char       ** const args)
{
    pid_t const pid = fork ();
    if (pid == 0) {
        int const res = execv (path, args);
        if (res == -1)
            logE_ (_func, "execv() failed: ", errnoString (errno));
        else
            logE_ (_func, "execv(): unexpected return value: ", res);

        exit (-1);
    }

    if (pid == -1) {
        logE_ (_func, "fork() failed: ", errnoString (errno));
        return Result::Failure;
    }

    return Result::Success;
}

static Result receiveSpawnerMessage (int      const s_unix,
                                     Byte   * const buf,
                                     Size     const buf_size,
                                     Size           pre_nread,
                                     int    * const fds,
                                     Size     const max_fds,
                                     Byte   * const cmsgbuf,
                                     Size     const cmsgbuf_len,
                                     Count  * const mt_nonnull ret_num_fds,
                                     Uint32 * const mt_nonnull ret_msg_len,
                                     Count  * const mt_nonnull ret_nread,
                                     bool   * const mt_nonnull ret_eof)
{
    *ret_num_fds = 0;
    *ret_msg_len = 0;
    *ret_nread = 0;
    *ret_eof = false;

    Size pos = 0;
    do {
        Size nread = 0;

        if (pre_nread == 0) {
            memset (cmsgbuf, 0, cmsgbuf_len);

            struct msghdr msg;
            memset (&msg, 0, sizeof (msg));
            msg.msg_control = cmsgbuf;
            msg.msg_controllen = cmsgbuf_len;

            struct iovec iovs [1];
            iovs [0].iov_base = buf + pos;
            iovs [0].iov_len  = buf_size - pos;
            msg.msg_iov = iovs;
            msg.msg_iovlen = 1;

            for (;;) {
                int const res = recvmsg (s_unix, &msg, 0);
                if (res == -1) {
                    if (errno == EINTR)
                        continue;

                    logE_ (_func, "recvmsg() failed: ", errnoString (errno));
                    return Result::Failure;
                }

                if (res == 0) {
                    logD_ (_func, "s_unix eof");
                    *ret_eof = true;
                    return Result::Success;
                }

                if (res < 0) {
                    logE_ (_func, "recvmsg(): unexpected return value: ", res);
                    return Result::Failure;
                }

                if ((Size) res > buf_size - pos) {
                    logE_ (_func, "recvmsg(): unexpected result ", res, " (too many bytes received)");
                    return Result::Failure;
                }

                nread = (Size) res;
                break;
            }

            if (msg.msg_flags & MSG_TRUNC ||
                msg.msg_flags & MSG_CTRUNC ||
              #ifdef __linux__
                msg.msg_flags & MSG_ERRQUEUE ||
              #endif
                msg.msg_flags & MSG_OOB)
            {
                logE_ (_func, "recvmsg(): unexpected msg.msg_flags: ", (unsigned int) msg.msg_flags);
                return Result::Failure;
            }

            struct cmsghdr * const cmsg = CMSG_FIRSTHDR (&msg);
            if (pos == 0) {
                Count num_fds = 0;
                if (cmsg) {
                    logD_ (_func, "cmsg");

                    if (cmsg->cmsg_level != SOL_SOCKET ||
                        cmsg->cmsg_type  != SCM_RIGHTS)
                    {
                        logE_ (_func, "recvmsg(): unexpected message: level ", cmsg->cmsg_level, " type ", cmsg->cmsg_type);
                        return Result::Failure;
                    }

                    if (CMSG_NXTHDR (&msg, cmsg)) {
                        logE_ (_func, "recvmsg(): unexpected extra cmsghdr");
                        return Result::Failure;
                    }

                    num_fds = (Count) buf [0];
                    logD_ (_func, "num_fds: ", num_fds);
                    if (num_fds > max_fds) {
                        logE_ (_func, "recvmsg(): too many fds: ", num_fds);
                        return Result::Failure;
                    }
                    for (unsigned i = 0; i < num_fds; ++i) {
                        logD_ (_func, "SCM_RIGHTS fd #", i, ": ", ((int const *) CMSG_DATA (cmsg)) [i]);
                        // TEST posixClose (((int const *) CMSG_DATA (cmsg)) [i], _func, "SCM_RIGHTS fd #", i);
                    }
                    memcpy (fds, (int const *) CMSG_DATA (cmsg), sizeof (int) * num_fds);
                }

                *ret_num_fds = num_fds;
            } else {
                if (cmsg) {
                    logE_ (_func, "recvmsg(): unexpected cmsghdr");
                    return Result::Failure;
                }
            }
        } else {
            nread = pre_nread;
            pre_nread = 0;
        }

        if (pos < 5) {
            if (pos + nread >= 5) {
                *ret_msg_len = ((Uint32) buf [1] << 24) |
                               ((Uint32) buf [2] << 16) |
                               ((Uint32) buf [3] <<  8) |
                               ((Uint32) buf [4] <<  0);
            }
        }

        *ret_nread += nread;
        pos += nread;
    } while (pos < 5 || pos < *ret_msg_len);

    return Result::Success;
}

static void processSpawnerChildProc (int const s_unix)
{
    Size const buf_size = 65536;
    Byte * const buf = new (std::nothrow) Byte [buf_size];
    assert (buf);

    unsigned const max_fds = 1024;
    int * const fds = new (std::nothrow) int [max_fds];
    assert (fds);

    Size const cmsgbuf_len = CMSG_SPACE (sizeof (int) * max_fds);
    Byte * const cmsgbuf = new (std::nothrow) Byte [cmsgbuf_len];
    assert (cmsgbuf);

    Size pre_nread = 0;
    for (;;) {
        Count  num_fds = 0;
        Uint32 msg_len = 0;
        Count  nread = 0;
        bool   eof = false;

        if (!receiveSpawnerMessage (s_unix,
                                    buf,
                                    buf_size,
                                    pre_nread,
                                    fds,
                                    max_fds,
                                    cmsgbuf,
                                    cmsgbuf_len,
                                    &num_fds,
                                    &msg_len,
                                    &nread,
                                    &eof))
        {
            logE_ (_func, "receiveSpawnerMessage() failed");
            goto _return;
        }

        if (eof) {
            logD_ (_func, "eof");
            goto _return;
        }

        if (msg_len > nread) {
            logE_ (_func, "bad msg_len ", msg_len, ", nread ", nread);
            goto _return;
        }

        if (msg_len < 5) {
            logE_ (_func, "message is too short, msg_len ", msg_len);
            goto _return;
        }

        Count num_args = 0;
        Count num_fd_subst_args = 0;
        for (Size i = 5; i < msg_len; ++i) {
            if (buf [i] == 0)
                ++num_args;
            else
            if (buf [i] == 1) {
                ++num_fd_subst_args;
                ++num_args;
            }
        }

        if (num_fd_subst_args > num_fds) {
            logE_ (_func, "num_fd_subst_args ", num_fd_subst_args, " > num_fds ", num_fds);
            goto _return;
        }

        logD_ (_func, "num_args ", num_args, ", num_fds ", num_fds);
        if (num_args > 0) {
            num_args += num_fds - num_fd_subst_args;

            char ** const args = new (std::nothrow) char* [num_args + 1];
            assert (args);

            Count arg_idx = 0;

            String* fd_subst_args [num_fd_subst_args];
            {
                Count fd_subst_idx = 0;
                Size arg_begin = 5;
                for (Size i = arg_begin; i < msg_len; ++i) {
                    if (buf [i] == 0 || buf [i] == 1) {
                        if (buf [i] == 0) {
                            args [arg_idx] = (char*) (buf + arg_begin);
                            logD_ (_func, "arg_begin ", arg_begin, " arg: ", args [arg_idx]);
                        } else
                        if (buf [i] == 1) {
                            StRef<String> str = makeString (fds [fd_subst_idx]);
                            fd_subst_args [fd_subst_idx] = str;
                            str.setNoUnref ((String*) NULL);

                            args [arg_idx] = fd_subst_args [fd_subst_idx]->cstr();
                            logD_ (_func, "fd subst arg: ", fd_subst_args [fd_subst_idx]);
                            ++fd_subst_idx;
                        }

                        ++arg_idx;
                        arg_begin = i + 1;
                    }
                }
            }

            String* fd_args [num_fds - num_fd_subst_args];
            for (Count i = num_fd_subst_args; i < num_fds - num_fd_subst_args; ++i) {
                StRef<String> str = makeString (fds [i]);
                fd_args [i] = str;
                str.setNoUnref ((String*) NULL);

                args [arg_idx] = fd_args [i]->cstr();
                logD_ (_func, "fd arg: ", fd_args [i]);
                ++arg_idx;
            }

            assert (arg_idx == num_args);
            args [num_args] = NULL;

            logD_ (_func, "spawning");
            if (!doSpawnProcess (args [0], args))
                logE_ (_func, "could not spawn process ", args [0]);

            delete[] args;

            for (Count i = 0; i < sizeof (fd_subst_args) / sizeof (fd_subst_args[0]); ++i)
                fd_subst_args [i]->unref ();

            for (Count i = 0; i < sizeof (fd_args) / sizeof (fd_args[0]); ++i)
                fd_args [i]->unref ();
        } else {
            logE_ (_func, "no args");
        }

        // We don't care about closing fds in error cases, because the process
        // will be terminated immediately.
        for (Count i = 0; i < num_fds; ++i)
            posixClose (fds [i], _func, "fds [", i, "] ");

        pre_nread = nread - msg_len;
        if (pre_nread > 0)
            memmove (buf, buf + msg_len, pre_nread);
    }

_return:
    delete[] cmsgbuf;
    delete[] fds;
    delete[] buf;
}

Result spawnProcess (ConstMemory         const path,
                     ConstMemory const * const args,
                     Count               const num_args,
                     int               * const fds,
                     Count               const num_fds)
{
    Buffer buffer;
    Count const buf_size_limit = 65536;
    {
        // 4 bytes for msg size
        // 1 byte for num_pass_fds
        // path + 1 byte
        // args[] + 1 byte for each arg
        Count total_len = 1 + 4;

        if (path.len() + 1 > buf_size_limit - total_len) {
            logE_ (_func, "path is too long: ", path.len());
            return Result::Failure;
        }
        total_len += path.len() + 1;

        for (Count i = 0; i < num_args; ++i) {
            if (args [i].len() + 1 > buf_size_limit - total_len) {
                logE_ (_func, "args are too long");
                return Result::Failure;
            }
            total_len += args [i].len() + 1;
        }

        buffer.allocate (total_len);
    }
    {
        Byte * const buf = buffer.mem.buf();
        Size pos = 0;

        assert (num_fds < 256);
        buf [pos] = (Byte) (num_fds);
        pos += 1;

        buf [pos + 0] = (Byte) (buffer.mem.len() >> 24) & 0xff;
        buf [pos + 1] = (Byte) (buffer.mem.len() >> 16) & 0xff;
        buf [pos + 2] = (Byte) (buffer.mem.len() >>  8) & 0xff;
        buf [pos + 3] = (Byte) (buffer.mem.len() >>  0) & 0xff;
        pos += 4;

        memcpy (buf + pos, path.buf(), path.len());
        pos += path.len();
        buf [pos] = 0;
        pos += 1;

        for (Count i = 0; i < num_args; ++i) {
            memcpy (buf + pos, args [i].buf(), args [i].len());
            pos += args [i].len();
            if (args [i].buf() == spawnProcess_fd_)
                buf [pos] = 1;
            else
                buf [pos] = 0;

            pos += 1;
        }
    }

    char cmsgbuf [CMSG_SPACE (num_fds * sizeof (int))];
    memset (cmsgbuf, 0, sizeof (cmsgbuf));

    struct msghdr msg;
    memset (&msg, 0, sizeof (struct msghdr));
    if (num_fds > 0) {
        msg.msg_control = cmsgbuf;
        msg.msg_controllen = sizeof cmsgbuf;

        struct cmsghdr * const cmsg = CMSG_FIRSTHDR (&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN (num_fds * sizeof (int));
        memcpy ((int*) CMSG_DATA (cmsg), fds, num_fds * sizeof (int));

        msg.msg_controllen = CMSG_SPACE (num_fds * sizeof (int));
    }

    struct iovec iovs [1];
    iovs [0].iov_base = buffer.mem.buf();
    iovs [0].iov_len  = buffer.mem.len();
    msg.msg_iov = iovs;
    msg.msg_iovlen = 1;

    for (;;) {
        // SUS p2.10 (Sockets/Socket Receive Queue) answers questions about
        // mixing regular and ancillary data.
        int const res = sendmsg (glob_s_unix, &msg,
              #ifdef __MACH__
                0
              #else
                MSG_NOSIGNAL
              #endif
                );
        if (res == -1) {
            if (errno == EINTR)
                continue;

            logE_ (_func, "sendmsg() failed: ", errnoString (errno));
            return Result::Failure;
        }

        if ((Size) res != buffer.mem.len()) {
            logE_ (_func, "sendmsg(): sent ", res, " bytes instead of ", buffer.mem.len());
            return Result::Failure;
        }

        break;
    }

    return Result::Success;
}

Result spawnProcessWithPipes (ConstMemory               const path,
                              ConstMemory             * const args,
                              Count                     const num_args,
                              Count                     const num_input_pipes,
                              ProcessInputPipe       ** const ret_input_pipes,
                              Count                     const num_output_pipes,
                              ProcessOutputPipe      ** const ret_output_pipes,
                              Count                     const num_async_input_pipes,
                              ProcessAsyncInputPipe  ** const ret_async_input_pipes,
                              Count                     const num_async_output_pipes,
                              ProcessAsyncOutputPipe ** const ret_async_output_pipes)
{
    int in_pipe_fds [num_input_pipes] [2];
    for (Count i = 0; i < num_input_pipes; ++i) {
        in_pipe_fds [i] [0] = -1;
        in_pipe_fds [i] [1] = -1;
    }

    int out_pipe_fds [num_output_pipes] [2];
    for (Count i = 0; i < num_output_pipes; ++i) {
        out_pipe_fds [i] [0] = -1;
        out_pipe_fds [i] [1] = -1;
    }

    int in_async_pipe_fds [num_async_input_pipes] [2];
    for (Count i = 0; i < num_async_input_pipes; ++i) {
        in_async_pipe_fds [i] [0] = -1;
        in_async_pipe_fds [i] [1] = -1;
    }

    int out_async_pipe_fds [num_async_output_pipes] [2];
    for (Count i = 0; i < num_async_output_pipes; ++i) {
        out_async_pipe_fds [i] [0] = -1;
        out_async_pipe_fds [i] [1] = -1;
    }

    for (Count i = 0; i < num_input_pipes; ++i) {
        int const res = pipe (in_pipe_fds [i]);
        if (res != 0) {
            if (res == -1)
                logE_ (_func, "pipe() failed: ", errnoString (errno));
            else
                logE_ (_func, "pipe(): unexpected return value: ", res);

            goto _failure;
        }
    }

    for (Count i = 0; i < num_output_pipes; ++i) {
        int const res = pipe (out_pipe_fds [i]);
        if (res != 0) {
            if (res == -1)
                logE_ (_func, "pipe() failed: ", errnoString (errno));
            else
                logE_ (_func, "pipe(): unexpected return value: ", res);

            goto _failure;
        }
    }

    for (Count i = 0 ; i < num_async_input_pipes; ++i) {
        if (!posix_createNonblockingPipe (&in_async_pipe_fds [i])) {
            logE_ (_func, "could not create async input pipe #", i);
            goto _failure;
        }
    }

    for (Count i = 0; i < num_async_output_pipes; ++i) {
        if (!posix_createNonblockingPipe (&out_async_pipe_fds [i])) {
            logE_ (_func, "could not create async output pipe #", i);
            goto _failure;
        }
    }

  {
    Count const total_pipes =   num_input_pipes
                              + num_output_pipes
                              + num_async_input_pipes
                              + num_async_output_pipes;

    {
        Count const num_fds = total_pipes;
        int fds [num_fds];
        {
            Count pos = 0;
            for (Count i = 0; i < num_input_pipes; ++i) {
                fds [pos] = in_pipe_fds [i] [1];
                ++pos;
            }
            for (Count i = 0; i < num_output_pipes; ++i) {
                fds [pos] = out_pipe_fds [i] [0];
                ++pos;
            }
            for (Count i = 0; i < num_async_input_pipes; ++i) {
                fds [pos] = in_async_pipe_fds [i] [1];
                ++pos;
            }
            for (Count i = 0; i < num_async_output_pipes; ++i) {
                fds [pos] = out_async_pipe_fds [i] [0];
                ++pos;
            }
        }

        if (!spawnProcess (path, args, num_args, fds, num_fds))
            goto _failure;
    }
  }

    for (Count i = 0; i < num_input_pipes; ++i) {
        ret_input_pipes [i]->init (in_pipe_fds [i] [0]);
        posixClose (in_pipe_fds [i] [1], _func, "in_pipe_fds [", i, "] [1] ");
    }
    for (Count i = 0; i < num_output_pipes; ++i) {
        ret_output_pipes [i]->init (out_pipe_fds [i] [1]);
        posixClose (out_pipe_fds [i] [0], _func, "out_pipe_fds [", i, "] [0] ");
    }
    for (Count i = 0; i < num_async_input_pipes; ++i) {
        ret_async_input_pipes [i]->init (in_async_pipe_fds [i] [0]);
        posixClose (in_async_pipe_fds [i] [1], _func, "in_async_pipe_fds [", i, "] [1] ");
    }
    for (Count i = 0; i < num_async_output_pipes; ++i) {
        ret_async_output_pipes [i]->init (out_async_pipe_fds [i] [1]);
        posixClose (out_async_pipe_fds [i] [0], _func, "out_async_pipe_fds [", i, "] [0] ");
    }

    return Result::Success;

_failure:
    for (Count i = 0; i < num_input_pipes; ++i) {
        posixClose (in_pipe_fds [i] [0], _func, "in_pipe_fds [", i, "] [0] ");
        posixClose (in_pipe_fds [i] [1], _func, "in_pipe_fds [", i, "] [1] ");
    }
    for (Count i = 0; i < num_output_pipes; ++i) {
        posixClose (out_pipe_fds [i] [0], _func, "out_pipe_fds [", i, "] [0] ");
        posixClose (out_pipe_fds [i] [1], _func, "out_pipe_fds [", i, "] [1] ");
    }
    for (Count i = 0; i < num_async_input_pipes; ++i) {
        posixClose (in_async_pipe_fds [i] [0], _func, "in_async_pipe_fds [", i, "] [0] ");
        posixClose (in_async_pipe_fds [i] [1], _func, "in_async_pipe_fds [", i, "] [1] ");
    }
    for (Count i = 0; i < num_async_output_pipes; ++i) {
        posixClose (out_async_pipe_fds [i] [0], _func, "out_async_pipe_fds [", i, "] [0] ");
        posixClose (out_async_pipe_fds [i] [1], _func, "out_async_pipe_fds [", i, "] [1] ");
    }

    return Result::Failure;
}

static void sigchldHandler (int const /* signum */)
{
    for (;;) {
        int const res = waitpid (-1, NULL, WNOHANG);
        if (res == -1) {
            if (errno == EINTR)
                continue;

            break;
        }

        if (res == 0)
            break;
    }
}

Result initProcessSpawner ()
{
    {
        struct sigaction sa;
        memset (&sa, 0, sizeof (sa));
        sa.sa_handler = sigchldHandler;
        sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;

        int const res = sigaction (SIGCHLD, &sa, NULL);
        if (res == -1) {
            logE_ (_func, "sigaction() failed: ", errnoString (errno));
            return Result::Failure;
        }

        if (res != 0) {
            logE_ (_func, "sigaction(): unexpected return value: ", res);
            return Result::Failure;
        }
    }

    int s_unix [2] = { -1, -1 };
    {
        // SOCK_DGRAM sockets do not notify when peer is closed (at least in case of
        // early peer termination).
        // SOCK_SEQPACKET is not supported on FreeBSD.
        int const res = socketpair (AF_UNIX, SOCK_STREAM, 0, s_unix);
        if (res == -1) {
            logE_ (_func, "socketpair() failed: ", errnoString (errno));
            return Result::Failure;
        } else
        if (res != 0) {
            logE_ (_func, "socketpair(): unexpected return value: ", res);
            return Result::Failure;
        }
    }

    // Forking a spawner process early to be able to fork clean processes later on.
    // Without this, we would have to fork from a heavy multi-threaded main process,
    // which is unsafe.
    pid_t const pid = fork ();
    if (pid == 0) {
        posixClose (s_unix [1], _func, "s_unix[1] ");
      // TODO set FD_CLOEXEC for pipe_fd [1] (or CLOEXEC for pipe syscall)

        processSpawnerChildProc (s_unix [0]);

        posixClose (s_unix [0], _func, "s_unix[0] ");
        exit (0);
    }

    if (pid == -1) {
        logE_ (_func, "fork() failed: ", errnoString (errno));
        posixClose (s_unix [0], _func, "s_unix[0] ");
        posixClose (s_unix [1], _func, "s_unix[1] ");
        return Result::Failure;
    }

    posixClose (s_unix [0], _func, "s_unix[0] ");
    glob_s_unix = s_unix [1];

    return Result::Success;
}

namespace {
class ExitPoll : public Object
{
private:
    Embed<NativeAsyncFile> file;

  mt_iface (AsyncInputStream::InputFrontend)
    static AsyncInputStream::InputFrontend const input_frontend;

    static void processInput (void *_self);

    static void processError (Exception *exc_,
                              void      *_self);
  mt_iface_end

public:
    mt_const Result init (PollGroup * mt_nonnull poll_group);

    ExitPoll (EmbedContainer * const embed_container)
        : Object (embed_container),
          file   (this /* embed_container */)
    {}
};
}

AsyncInputStream::InputFrontend const ExitPoll::input_frontend = {
    processInput,
    processError
};

void
ExitPoll::processInput (void * const _self)
{
    logD_ (_func_);

    ExitPoll * const self = static_cast <ExitPoll*> (_self);

    Byte buf [65536];
    for (;;) {
        Size bytes_read = 0;
//#warning This makes no sense, because we're' dealing with the write end of the pipe.
        AsyncIoResult const res = self->file->read (Memory::forObject (buf), &bytes_read);

        if (res.isEof() || res == AsyncIoResult::Error) {
            if (res == AsyncIoResult::Error)
                logD_ (_func, "exception: ", exc->toString());
            else
                logD_ (_func, "eof");

            exit (EXIT_FAILURE);
            break;
        }

        if (res.isAgain())
            break;
    }
}

void
ExitPoll::processError (Exception * const exc_,
                        void      * const /* _self */)
{
    if (exc_)
        logD_ (_func, "exception: ", exc_->toString());
    else
        logD_ (_func_);

    exit (EXIT_FAILURE);
}

mt_const Result
ExitPoll::init (PollGroup * const mt_nonnull poll_group)
{
    logD_ (_func_);

    file->setInputFrontend (CbDesc<AsyncInputStream::InputFrontend> (&input_frontend, this, this));
    file->setFd (glob_s_unix);

    // TODO Send empty message to catch early errors.
    if (!poll_group->addPollable (file->getPollable())) {
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

Result initProcessSpawner_addExitPoll (PollGroup * const mt_nonnull poll_group)
{
    // Never freed
    ExitPoll * const exit_poll = new (std::nothrow) ExitPoll (NULL /* embed_container */);
    assert (exit_poll);

    if (!exit_poll->init (poll_group)) {
        logE_ (_func, "ExitPoll::init() failed");
        return Result::Failure;
    }

    return Result::Success;
}

}

