/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/util_dev.h>

#include <libmary/message_service.h>


namespace M {

MessageService::Session::Session (EmbedContainer * const embed_container)
    : Object (embed_container),
      destroyed  (false),
      tcp_conn   (this /* embed_container */),
      sender     (this /* embed_container */),
      receiver   (this /* embed_container */),
      msg_server (this /* embed_container */),
      session_list_el (NULL)
{
}

Sender::Frontend const MessageService::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
MessageService::senderStateChanged (SenderState   const sender_state,
                                    void        * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<MessageService> const self = session->weak_message_service.getRef ();
    if (!self)
        return;

    logD_ (_func, "sender_state ", sender_state);
}

void
MessageService::senderClosed (Exception * const /* exc_ */,
                              void      * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<MessageService> const self = session->weak_message_service.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    mt_unlocks (mutex) self->destroySession (session);
}

MessageServer::Frontend const MessageService::msg_server_frontend = {
    message,
    msgClosed
};

void
MessageService::message (Memory   const msg,
                         void   * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<MessageService> const self = session->weak_message_service.getRef ();
    if (!self)
        return;

    logD_ (_func, ":");
    hexdump (msg);

    if (session->events_cb) {
        session->events_cb.call (session->events_cb->message,
                                 /*(*/ msg, session->client_addr, session->sender.ptr() /*)*/);
    } else
    if (self->default_events_cb) {
        self->default_events_cb.call (self->default_events_cb->message,
                                      /*(*/ msg, session->client_addr, session->sender.ptr() /*)*/);
    }
}

void
MessageService::msgClosed (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<MessageService> const self = session->weak_message_service.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    mt_unlocks (mutex) self->destroySession (session);
}

mt_unlocks (mutex) void
MessageService::destroySession (Session * const mt_nonnull session)
{
    if (session->destroyed) {
        mutex.unlock ();
        return;
    }
    session->destroyed = true;

    if (session->pollable_key) {
        if (Ref<ServerThreadContext> const thread_ctx = session->weak_thread_ctx.getRef ()) {
            thread_ctx->getPollGroup()->removePollable (session->pollable_key);
        }
        session->pollable_key = NULL;
    }

    if (session->session_list_el) {
        session_list.remove (session->session_list_el);
        session->session_list_el = NULL;
    }
    mutex.unlock ();

    if (session->events_cb)
        session->events_cb.call (session->events_cb->closed);
}

void
MessageService::sessionLifetimeTimeout (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<MessageService> const self = session->weak_message_service.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    mt_unlocks (mutex) self->destroySession (session);
}

bool
MessageService::acceptOneConnection ()
{
    Session * const session = grab (new (std::nothrow) Session (NULL /* embed_container */));

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

    session->weak_message_service = this;
    session->client_addr = client_addr;

    Ref<ServerThreadContext> const thread_ctx = server_ctx->selectThreadContext ();
    session->weak_thread_ctx = thread_ctx;

    session->sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, session, session));
    session->sender->init (thread_ctx->getDeferredProcessor());
    session->sender->setConnection (session->tcp_conn);
    session->receiver->init (session->tcp_conn,
                             thread_ctx->getDeferredProcessor());
    session->msg_server->init (session->receiver,
                               CbDesc<MessageServer::Frontend> (&msg_server_frontend, session, session),
                               65536 /* max_msg_len */);

    if (frontend) {
        frontend.call (frontend->connected, 
                       /*(*/ session, client_addr, session->sender.ptr(), &session->events_cb /*)*/);
    }

    mutex.lock ();
    if (session->destroyed) {
      // closeSession() has been called by the user
        mutex.unlock ();
        return true;
    }

    session->pollable_key = thread_ctx->getPollGroup()->addPollable (session->tcp_conn->getPollable());
    if (!session->pollable_key) {
        logE_ (_func, "addPollable() failed: ", exc->toString());
        mt_unlocks (mutex) destroySession (session);
        return true;
    }

    session->session_list_el = session_list.append (session);
    mutex.unlock ();

    if (session->events_cb)
        session->events_cb.call (session->events_cb->readyToSend);

    session->receiver->start ();

    if (max_session_lifetime_millisec != 0) {
        thread_ctx->getTimers()->addTimer_microseconds (
                CbDesc<Timers::TimerCallback> (sessionLifetimeTimeout, session, session),
                max_session_lifetime_millisec * 1000,
                false /* periodical*/,
                true  /* auto_delete */,
                true  /* delete_after_tick */);
    }

    return true;
}

TcpServer::Frontend const MessageService::server_frontend = {
    accepted
};

void
MessageService::accepted (void * const _self)
{
    MessageService * const self = static_cast <MessageService*> (_self);

    for (;;) {
        if (!self->acceptOneConnection ())
            break;
    }
}

mt_throws Result
MessageService::bind (IpAddress const addr)
{
    return tcp_server->bind (addr);
}

mt_throws Result
MessageService::start ()
{
    if (!tcp_server->listen ()) {
        logE_ (_func, "tcp_server.listen() failed: ", exc->toString());
        return Result::Failure;
    }

    mutex.lock ();
    server_pollable_key = server_ctx->getMainThreadContext()->getPollGroup()->addPollable (tcp_server->getPollable());
    if (!server_pollable_key) {
        mutex.unlock ();
        logE_ (_func, "addPollable() failed: ", exc->toString());
        return Result::Failure;
    }
    mutex.unlock ();

    if (!tcp_server->start ()) {
        logE_ (_func, "tcp_server.start() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

mt_const mt_throws Result
MessageService::init (ServerContext               * const mt_nonnull server_ctx,
                      CbDesc<Frontend>      const &frontend,
                      CbDesc<SessionEvents> const &default_events_cb,
                      Time                          const max_session_lifetime_millisec)
{
    this->server_ctx = server_ctx;
    this->frontend = frontend;
    this->default_events_cb = default_events_cb;
    this->max_session_lifetime_millisec = max_session_lifetime_millisec;

    tcp_server->init (CbDesc<TcpServer::Frontend> (&server_frontend, this, this),
                      server_ctx->getMainThreadContext()->getDeferredProcessor(),
                      server_ctx->getMainThreadContext()->getTimers());

    if (!tcp_server->open ()) {
        logE_ (_func, "tcp_server.open() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

MessageService::MessageService (EmbedContainer * const embed_container)
    : Object (embed_container),
      max_session_lifetime_millisec (0),
      tcp_server (this /* embed_container */)
{
}

MessageService::~MessageService ()
{
    mutex.lock ();
    {
        while (!session_list.isEmpty()) {
            Ref<Session> const session = session_list.getFirst();
            mt_unlocks (destroySession (session));
            mutex.lock ();
        }
    }

    if (server_pollable_key) {
        server_ctx->getMainThreadContext()->getPollGroup()->removePollable (server_pollable_key);
        server_pollable_key = NULL;
    }
    mutex.unlock ();
}

}

