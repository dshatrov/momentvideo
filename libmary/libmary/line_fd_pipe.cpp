/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>

#include <libmary/line_fd_pipe.h>


namespace M {

LineServer::Frontend const LineFdPipe::line_frontend = {
    line,
    closed
};

void
LineFdPipe::line (ConstMemory   const line,
                  void        * const _self)
{
    LineFdPipe * const self = static_cast <LineFdPipe*> (_self);

    if (self->frontend)
        self->frontend.call (self->frontend->line, line);
}

void
LineFdPipe::closed (void * const _self)
{
    LineFdPipe * const self = static_cast <LineFdPipe*> (_self);

    self->doRelease ();

    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

mt_const mt_throws Result
LineFdPipe::init (int                 const fd,
                  CbDesc<Frontend>    const &frontend,
                  PollGroup         * const mt_nonnull poll_group,
                  DeferredProcessor * const mt_nonnull deferred_processor,
                  Size                const max_line_len)
{
    this->frontend = frontend;
    this->poll_group = poll_group;

    line_file->setFd (fd);
    line_file_opened = true;

    line_receiver->init (line_file,
                         deferred_processor);
    line_server->init (line_receiver,
                       CbDesc<LineServer::Frontend> (&line_frontend, this, this),
                       max_line_len);

    mutex.lock ();
    pollable_key = poll_group->addPollable (line_file->getPollable());
    if (!pollable_key) {
        mutex.unlock ();
        logE_ (_this_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }
    mutex.unlock ();

    line_receiver->start ();

    return Result::Success;
}

void
LineFdPipe::doRelease ()
{
    mutex.lock ();
    PollGroup::PollableKey const tmp_key = pollable_key;
    pollable_key = NULL;
    mutex.unlock ();

    if (tmp_key)
        poll_group->removePollable (tmp_key);
}

LineFdPipe::LineFdPipe (EmbedContainer * const embed_container)
    : Object           (embed_container),
      line_file_opened (false),
      line_file        (this /* embed_container */),
      line_receiver    (this /* embed_container */),
      line_server      (this /* embed_container */)
{
}

LineFdPipe::~LineFdPipe ()
{
    doRelease ();

    if (line_file_opened) {
        if (!line_file->close (false /* flush_data */))
            logE_ (_func, "could not close pipe file: ", exc->toString());
    }
}

}

