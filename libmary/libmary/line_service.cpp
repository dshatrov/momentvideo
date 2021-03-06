/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/line_service.h>


namespace M {

LineService::LineConnection::LineConnection (EmbedContainer * const embed_container)
    : Object (embed_container),
      valid (true),
      tcp_conn      (this /* embed_container */),
      conn_receiver (this /* embed_container */),
      line_server   (this /* embed_container */)
{
}

LineService::LineConnection::~LineConnection ()
{
}

void
LineService::releaseLineConnection (LineConnection * const mt_nonnull line_conn)
{
    thread_ctx->getPollGroup()->removePollable (line_conn->pollable_key);
}

bool
LineService::acceptOneConnection ()
{
    Ref<LineConnection> const line_conn = grab (new (std::nothrow) LineConnection (NULL /* embed_container */));
    line_conn->weak_line_service = this;

    IpAddress client_addr;
    {
	TcpServer::AcceptResult const res = tcp_server->accept (line_conn->tcp_conn,
                                                                &client_addr);
	if (res == TcpServer::AcceptResult::Error) {
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted)
	    return false;

	assert (res == TcpServer::AcceptResult::Accepted);
    }

    line_conn->conn_receiver->init (line_conn->tcp_conn,
                                    thread_ctx->getDeferredProcessor());
    line_conn->line_server->init (line_conn->conn_receiver,
                                  CbDesc<LineServer::Frontend> (&line_server_frontend, this, this),
                                  max_line_len);

    mutex.lock ();
    line_conn->pollable_key =
            thread_ctx->getPollGroup()->addPollable (line_conn->tcp_conn->getPollable());
    if (!line_conn->pollable_key) {
	mutex.unlock ();
	logE_ (_func, exc->toString());
	return true;
    }

    line_conn->conn_list_el = conn_list.append (line_conn);
    mutex.unlock ();

    line_conn->conn_receiver->start ();
    return true;
}

TcpServer::Frontend const LineService::tcp_server_frontend = {
    accepted
};

void
LineService::accepted (void * const _self)
{
    LineService * const self = static_cast <LineService*> (_self);

    for (;;) {
        if (!self->acceptOneConnection ())
            break;
    }
}

LineServer::Frontend const LineService::line_server_frontend = {
    line,
    closed
};

void
LineService::line (ConstMemory   const line,
                   void        * const _self)
{
    LineService * const self = static_cast <LineService*> (_self);

    if (self->frontend)
        self->frontend.call (self->frontend->line, line);
}

void
LineService::closed (void * const _line_conn)
{
    LineConnection * const line_conn = static_cast <LineConnection*> (_line_conn);
    Ref<LineService> const self = line_conn->weak_line_service.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    if (!line_conn->valid) {
        self->mutex.unlock ();
        return;
    }
    line_conn->valid = false;

    self->releaseLineConnection (line_conn);
    self->conn_list.remove (line_conn->conn_list_el);
    line_conn->conn_list_el = NULL;

    self->mutex.unlock ();
}

mt_throws Result
LineService::init (ServerContext    * const mt_nonnull server_ctx,
                   CbDesc<Frontend>   const &frontend,
                   Size               const  max_line_len)

{
    this->thread_ctx = server_ctx->getMainThreadContext();
    this->frontend = frontend;
    this->max_line_len = max_line_len;

    if (!tcp_server->open ())
        return Result::Failure;

    tcp_server->init (
            CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, this),
            server_ctx->getMainThreadContext()->getDeferredProcessor(),
            server_ctx->getMainThreadContext()->getTimers());

    return Result::Success;
}

mt_throws Result
LineService::bind (IpAddress const addr)
{
    if (!tcp_server->bind (addr))
        return Result::Failure;

    return Result::Success;
}

mt_throws Result
LineService::start ()
{
    if (!tcp_server->listen ())
        return Result::Failure;

    mutex.lock ();
    assert (!server_pollable_key);
    server_pollable_key = thread_ctx->getPollGroup()->addPollable (tcp_server->getPollable());
    if (!server_pollable_key) {
        mutex.unlock ();
        return Result::Failure;
    }
    mutex.unlock ();

    if (!tcp_server->start ()) {
        logF_ (_func, "tcp_server.start() failed: ", exc->toString());

        mutex.lock ();
        thread_ctx->getPollGroup()->removePollable (server_pollable_key);
        server_pollable_key = NULL;
        mutex.unlock ();

        return Result::Failure;
    }

    return Result::Success;
}

LineService::LineService (EmbedContainer * const embed_container)
    : Object (embed_container),
      max_line_len (4096),
      tcp_server (this /* embed_container */)
{
}

LineService::~LineService ()
{
    mutex.lock ();

    if (server_pollable_key) {
        thread_ctx->getPollGroup()->removePollable (server_pollable_key);
        server_pollable_key = NULL;
    }

    {
        LineConnectionList::iter iter (conn_list);
        while (!conn_list.iter_done (iter)) {
            LineConnection * const line_conn = conn_list.iter_next (iter)->data;
            assert (line_conn->valid);
            line_conn->valid = false;
            releaseLineConnection (line_conn);
            line_conn->conn_list_el = NULL;
        }
        conn_list.clear ();
    }

    mutex.unlock ();
}

}

