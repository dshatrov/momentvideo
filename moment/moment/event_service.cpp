/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/event_service.h>


namespace Moment {

EventService::Session::Session (EmbedContainer * const embed_container)
    : Object      (embed_container),
    #ifndef LIBMARY_PLATFORM_WIN32
      async_file  (this /* embed_container */),
    #endif
      tcp_conn    (this /* embed_container */),
      sender      (this /* embed_container */),
      receiver    (this /* embed_container */),
      line_server (this /* embed_container */),
      session_list_el (NULL)
{
}

Sender::Frontend const EventService::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
EventService::senderStateChanged (SenderState   const sender_state,
                                  void        * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<EventService> const self = session->weak_event_service.getRef ();
    if (!self)
        return;

    logD_ (_self_func, "sender_state ", sender_state);
}

void
EventService::senderClosed (Exception * const exc_,
                            void      * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<EventService> const self = session->weak_event_service.getRef ();
    if (!self)
        return;

    logD_ (_func, "session 0x", fmt_hex, (UintPtr) _session, ": ",
           (exc_ ? ConstMemory (exc_->toString()->mem()) : ConstMemory()));

    self->mutex.lock ();
    self->destroySession (session);
    self->mutex.unlock ();
}

LineServer::Frontend const EventService::line_server_frontend = {
    line,
    lineClosed
};

void
EventService::line (ConstMemory   const line,
                    void        * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<EventService> const self = session->weak_event_service.getRef ();
    if (!self)
        return;

    logD_ (_self_func, "line: ", line);
}

void
EventService::lineClosed (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<EventService> const self = session->weak_event_service.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    self->destroySession (session);
    self->mutex.unlock ();
}

bool
EventService::acceptOneConnection ()
{
    Ref<Session> const session = grab (new (std::nothrow) Session (NULL /* embed_container */));

    IpAddress client_addr;
    {
        TcpServer::AcceptResult const res = tcp_server->accept (session->tcp_conn,
                                                                &client_addr);
        if (res == TcpServer::AcceptResult::Error) {
            logE_ (_func, "tcp_server.accept() failed: ", exc->toString());
            return false;
        }

        if (res == TcpServer::AcceptResult::NotAccepted)
            return false;

        assert (res == TcpServer::AcceptResult::Accepted);
    }

    session->weak_event_service = this;

    Ref<ServerThreadContext> const thread_ctx = server_ctx->selectThreadContext ();
    session->weak_thread_ctx = thread_ctx;

    session->sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, session, session));
    session->sender->setConnection (session->tcp_conn);
    session->sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    session->receiver->init (session->tcp_conn,
                             thread_ctx->getDeferredProcessor());
    session->line_server->init (session->receiver,
                                CbDesc<LineServer::Frontend> (&line_server_frontend, session, session));

    mutex.lock ();
    session->pollable_key = thread_ctx->getPollGroup()->addPollable (session->tcp_conn->getPollable());
    if (!session->pollable_key) {
        mutex.unlock ();
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return true;
    }

    session->session_list_el = session_list.append (session);
    mutex.unlock ();

    session->receiver->start ();
    return true;
}

TcpServer::Frontend const EventService::tcp_server_frontend = {
    accepted
};

void
EventService::accepted (void * const _self)
{
    EventService * const self = static_cast <EventService*> (_self);

    for (;;) {
        if (!self->acceptOneConnection ())
            break;
    }
}

mt_mutex (mutex) void
EventService::destroySession (Session * const mt_nonnull session)
{
    if (Ref<ServerThreadContext> const thread_ctx = session->weak_thread_ctx.getRef ()) {
        thread_ctx->getPollGroup()->removePollable (session->pollable_key);
    }
    session->pollable_key = NULL;

    if (session->session_list_el) {
        session_list.remove (session->session_list_el);
        session->session_list_el = NULL;
    }
}

#ifndef LIBMARY_PLATFORM_WIN32
mt_throws Result
EventService::addOnewayPipeFd (int const fd)
{
    Ref<Session> const session = grab (new (std::nothrow) Session (NULL /* embed_container */));

    session->weak_event_service = this;

    Ref<ServerThreadContext> const thread_ctx = server_ctx->selectThreadContext ();
    session->weak_thread_ctx = thread_ctx;

    session->async_file->setFd (fd);

    session->sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, session, session));
    session->sender->setConnection (session->async_file);
    session->sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());

    mutex.lock ();
    session->pollable_key = thread_ctx->getPollGroup()->addPollable (session->async_file->getPollable());
    if (!session->pollable_key) {
        mutex.unlock ();
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }

    session->session_list_el = session_list.append (session);
    mutex.unlock ();

    return Result::Success;
}
#endif

void
EventService::sendEvent (ConstMemory const line)
{
    mutex.lock ();
    {
        List< Ref<Session> >::iterator iter (session_list);
        while (!iter.done()) {
            Ref<Session> const &session = iter.next()->data;
            session->sender->send (page_pool, true /* do_flush */, line, "\n");
        }
    }
    mutex.unlock ();
}

mt_throws Result
EventService::bind (IpAddress const addr)
{
    return tcp_server->bind (addr);
}

mt_throws Result
EventService::start ()
{
    if (!tcp_server->listen ()) {
        logE_ (_this_func, "tcp_server.listen() failed: ", exc->toString());
        return Result::Failure;
    }

    mutex.lock ();
    server_pollable_key = server_ctx->getMainThreadContext()->getPollGroup()->addPollable (tcp_server->getPollable());
    if (!server_pollable_key) {
        mutex.unlock ();
        logE_ (_this_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }
    mutex.unlock ();

    if (!tcp_server->start ()) {
        logE_ (_this_func, "tcp_server.start() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

mt_const void
EventService::init (ServerContext * const mt_nonnull server_ctx,
                    PagePool      * const mt_nonnull page_pool)
{
    this->server_ctx = server_ctx;
    this->page_pool = page_pool;
}

mt_const mt_throws Result
EventService::initTcpServer ()
{
    tcp_server->init (CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, this),
                      server_ctx->getMainThreadContext()->getDeferredProcessor(),
                      server_ctx->getMainThreadContext()->getTimers());

    if (!tcp_server->open ()) {
        logE_ (_this_func, "tcp_server.open() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

EventService::EventService (EmbedContainer * const embed_container)
    : Object     (embed_container),
      tcp_server (this /* embed_container */)
{
}

EventService::~EventService ()
{
    mutex.lock ();
    {
        List< Ref<Session> >::iterator iter (session_list);
        while (!iter.done()) {
            Ref<Session> const &session = iter.next()->data;
            destroySession (session);
        }
        assert (session_list.isEmpty());
    }

    if (server_pollable_key) {
        server_ctx->getMainThreadContext()->getPollGroup()->removePollable (server_pollable_key);
        server_pollable_key = NULL;
    }
    mutex.unlock ();
}

}

