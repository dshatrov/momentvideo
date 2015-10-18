/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>
#include <libmary/util_dev.h>

#include <libmary/http_service.h>


// TODO Use selectThreadContext() for multithreading


namespace M {

static LogGroup libMary_logGroup_http_service ("http_service", LogLevel::D);

HttpService::HttpConnection::HttpConnection (EmbedContainer * const embed_container)
    : Object          (embed_container),
      valid           (true),
      tcp_conn        (this /* embed_container */),
      sender          (this /* embed_container */),
      receiver        (this /* embed_container */),
      http_server     (this /* embed_container */),
      pollable_key    (NULL),
      receiving_body  (false),
      preassembly_buf (NULL)
{
    logD (http_service, _this_func_);
}

HttpService::HttpConnection::~HttpConnection ()
{
    logD (http_service, _this_func_);

    if (preassembly_buf)
	delete[] preassembly_buf;
}

mt_mutex (mutex) void
HttpService::releaseHttpConnection (HttpConnection * const mt_nonnull http_conn)
{
  // TODO Invoke cur_handler callbacks to finalize current request?

    if (http_conn->conn_keepalive_timer) {
	timers->deleteTimer (http_conn->conn_keepalive_timer);
	http_conn->conn_keepalive_timer = NULL;
    }

    poll_group->removePollable (http_conn->pollable_key);
}

void
HttpService::destroyHttpConnection (HttpConnection * const mt_nonnull http_conn)
{
    mutex.lock ();
    if (!http_conn->valid) {
	mutex.unlock ();
	return;
    }
    http_conn->valid = false;

    releaseHttpConnection (http_conn);
    conn_list.remove (http_conn);
    mutex.unlock ();

    http_conn->unref ();
}

void
HttpService::unblockInput (void * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);
    http_conn->http_server->unblockInput ();
}

void
HttpService::connKeepaliveTimerExpired (void * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logD (http_service, _func, "0x", fmt_hex, (UintPtr) (http_conn));

    Ref<HttpService> const self = http_conn->weak_http_service.getRef ();
    if (!self)
        return;

    // Timers belong to the same thread as PollGroup, hence this call is safe.
    self->doCloseHttpConnection (http_conn, NULL /* req */);
}

HttpServer::Frontend const HttpService::http_frontend = {
    NULL /* rawData */,
    httpRequest,
    httpMessageBody,
    httpClosed
};

void
HttpService::httpRequest (HttpRequest * const mt_nonnull req,
                          bool        * const mt_nonnull ret_block_input,
			  void        * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logD_ (_func, "http_conn 0x", fmt_hex, (UintPtr) http_conn, ": ", req->getRequestLine());

    Ref<HttpService> const self = http_conn->weak_http_service.getRef ();
    if (!self)
        return;

    http_conn->cur_handler = NULL;
    http_conn->cur_msg_data = NULL;

    self->mutex.lock ();

    if (self->no_keepalive_conns)
	req->setKeepalive (false);

    if (http_conn->conn_keepalive_timer) {
//#warning fix race
        // FIXME Race condition: the timer might have just expired
        //       and an assertion in Timers::restartTimer() will be hit.
        self->timers->restartTimer (http_conn->conn_keepalive_timer);
    }

  // Searching for a handler with the longest matching path.
  //
  //     /a/b/c/  - last path element should be empty;
  //     /a/b/c   - last path element is "c".

    NamespaceContainer<HandlerEntry>::Namespace *cur_namespace = &self->namespace_container.root_namespace;
    Count const num_path_els = req->getNumPathElems();
    ConstMemory handler_path_el;
    Count handler_path_el_idx = 0;
    for (Count i = 0; i < num_path_els; ++i) {
	ConstMemory const path_el = req->getPath (i);
	NamespaceContainer<HandlerEntry>::NamespaceHash::EntryKey const namespace_key = cur_namespace->namespace_hash.lookup (path_el);
	if (!namespace_key) {
	    handler_path_el = path_el;
            handler_path_el_idx = i;
	    break;
	}

	cur_namespace = namespace_key.getDataPtr();
	assert (cur_namespace);
    }

    NamespaceContainer<HandlerEntry>::EntryHash::EntryKey handler_key =
            cur_namespace->entry_hash.lookup (handler_path_el);

    bool root_handler = false;
    if (!handler_key) {
	handler_key = cur_namespace->entry_hash.lookup (ConstMemory());
        if (handler_key)
            root_handler = true;
    }

    if (!handler_key) {
	self->mutex.unlock ();
	logD (http_service, _func, "No suitable handler found");

	ConstMemory const reply_body = "404 Not Found";

	Byte date_buf [unixtimeToString_BufSize];
	Size const date_len = unixtimeToString (Memory::forObject (date_buf), getUnixtime());
	logD (http_service, _func, "page_pool: 0x", fmt_hex, (UintPtr) self->page_pool.ptr());
	http_conn->sender->send (
		self->page_pool,
		true /* do_flush */,
		"HTTP/1.1 404 Not found\r\n"
		"Server: Moment/1.0\r\n"
		"Date: ", ConstMemory (date_buf, date_len), "\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: ", reply_body.len(), "\r\n"
		"\r\n",
		reply_body);

	if (!req->getKeepalive())
            http_conn->sender->closeAfterFlush ();

	return;
    }

    if (!root_handler
        && num_path_els > 0
        && handler_path_el_idx + 1 == num_path_els
        && req->getPath (0).len() /* excluding root "/" */)
    {
      // Adding a trailing slash to the path and redirecting the client.
      // This makes both "http://a.b/c" and "http://a.b/c/" work as expected
      // in most cases.

        self->mutex.unlock ();

        // Note that "location uri must be absolute" rule is not honoured here.
        http_conn->sender->send (
                self->page_pool,
                true /* do_flush */,
                "HTTP/1.1 302 Found\r\n"
                "Location: /", req->getFullPath(), "/\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 0\r\n"
                "\r\n");

        if (!req->getKeepalive())
            http_conn->sender->closeAfterFlush();

        logA_ ("http 302 ", req->getClientAddress(), " ", req->getRequestLine());

        return;
    }

    HandlerEntry * const handler = &handler_key.getDataPtr()->data;

    // Note: We count on the fact that handler entries are never removed during
    // lifetime of HttpService. This may change in the future, in which case
    // we'll have to add an extra reference to handler entry here.
    self->mutex.unlock ();

    logD (http_service, _func, "handler: 0x", fmt_hex, (UintPtr) handler);
    if (req->hasBody())
        http_conn->cur_handler = handler;

    http_conn->receiving_body = false;
    http_conn->preassembled_len = 0;

    if (!handler->preassembly || !req->hasBody()) {
        if (handler->cb && handler->cb->httpRequest) {
            HttpConnectionInfo conn_info (http_conn, ret_block_input);
            Result res = Result::Failure;
            if (!handler->cb.call_ret<Result> (
                        &res,
                        handler->cb->httpRequest,
                        /*(*/
                            req,
                            &conn_info,
                            http_conn->local_addr,
                            http_conn->sender.ptr(),
                            Memory(),
                            &http_conn->cur_msg_data
                        /*)*/)
                || !res)
            {
                http_conn->cur_handler = NULL;
            }

            if (http_conn->cur_msg_data && !req->hasBody())
                logW_ (_func, "msg_data is likely lost");

            http_conn->receiving_body = true;
        }
    }
}

void
HttpService::httpMessageBody (HttpRequest * const mt_nonnull req,
			      Memory        const mem,
			      bool          const end_of_request,
			      Size        * const mt_nonnull ret_accepted,
                              bool        * const mt_nonnull ret_block_input,
			      void        * const  _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    if (!http_conn->cur_handler) {
	*ret_accepted = mem.len();
	return;
    }

    if (http_conn->cur_handler->preassembly
	&& http_conn->preassembled_len < http_conn->cur_handler->preassembly_limit)
    {
        // 'size' is how much we'are going to preassemble for this request.
	Size size = http_conn->cur_handler->preassembly_limit;
        if (req->getContentLengthSpecified() && req->getContentLength() < size)
            size = req->getContentLength();

	bool alloc_new = true;
	if (http_conn->preassembly_buf) {
	    if (http_conn->preassembly_buf_size >= size)
		alloc_new = false;
	    else
		delete[] http_conn->preassembly_buf;
	}

	if (alloc_new) {
	    http_conn->preassembly_buf = new (std::nothrow) Byte [size];
            assert (http_conn->preassembly_buf);
	    http_conn->preassembly_buf_size = size;
	}

	if (mem.len() + http_conn->preassembled_len >= size
            || end_of_request)
        {
            {
                Size tocopy = size - http_conn->preassembled_len;
                if (tocopy > mem.len())
                    tocopy = mem.len();

		memcpy (http_conn->preassembly_buf + http_conn->preassembled_len,
			mem.mem(),
                        tocopy);

		*ret_accepted = tocopy;
		http_conn->preassembled_len += tocopy;
            }

            if (http_conn->cur_handler->parse_body_params) {
                req->parseParameters (
                        Memory (http_conn->preassembly_buf,
                                http_conn->preassembled_len));
            }

            if (http_conn->cur_handler->cb && http_conn->cur_handler->cb->httpRequest) {
                HttpConnectionInfo conn_info (http_conn, ret_block_input);
                Result res = Result::Failure;
                if (!http_conn->cur_handler->cb.call_ret<Result> (
                            &res,
                            http_conn->cur_handler->cb->httpRequest,
                            /*(*/
                                req,
                                &conn_info,
                                http_conn->local_addr,
                                http_conn->sender.ptr(),
                                Memory (http_conn->preassembly_buf,
                                        http_conn->preassembled_len),
                                &http_conn->cur_msg_data
                            /*)*/)
                    || !res)
                {
                    http_conn->cur_handler = NULL;
                }
            }
            http_conn->receiving_body = true;

            if (http_conn->cur_handler) {
                if (*ret_accepted < mem.len()) {
                    HttpConnectionInfo conn_info (http_conn, ret_block_input);
                    Size accepted = 0;
                    Result res = Result::Failure;
                    if (!http_conn->cur_handler->cb.call_ret<Result> (
                                &res,
                                http_conn->cur_handler->cb->httpMessageBody,
                                /*(*/
                                    req,
                                    &conn_info,
                                    http_conn->sender.ptr(),
                                    mem.region (*ret_accepted),
                                    end_of_request,
                                    &accepted,
                                    http_conn->cur_msg_data
                                /*)*/)
                        || !res)
                    {
                        http_conn->cur_handler = NULL;
                    }

                    *ret_accepted += accepted;
                } else {
                    if (end_of_request && !req->getContentLengthSpecified()) {
                        HttpConnectionInfo conn_info (http_conn, ret_block_input);
                        Size dummy_accepted = 0;
                        Result res = Result::Failure;
                        if (!http_conn->cur_handler->cb.call_ret<Result> (
                                    &res,
                                    http_conn->cur_handler->cb->httpMessageBody,
                                    /*(*/
                                        req,
                                        &conn_info,
                                        http_conn->sender.ptr(),
                                        Memory(),
                                        true /* end_of_request */,
                                        &dummy_accepted,
                                        http_conn->cur_msg_data
                                    /*)*/)
                            || !res)
                        {
                            http_conn->cur_handler = NULL;
                        }
                    }
                }
            }

            if (!http_conn->cur_handler)
                *ret_accepted = mem.len();
	} else {
	    memcpy (http_conn->preassembly_buf + http_conn->preassembled_len,
		    mem.mem(),
		    mem.len());
	    *ret_accepted = mem.len();
	    http_conn->preassembled_len += mem.len();
	}

        if (end_of_request)
            http_conn->cur_handler = NULL;

	return;
    }

    {
        HttpConnectionInfo conn_info (http_conn, ret_block_input);
        Result res = Result::Failure;
        if (!http_conn->cur_handler->cb.call_ret<Result> (
                    &res,
                    http_conn->cur_handler->cb->httpMessageBody,
                    /*(*/
                        req,
                        &conn_info,
                        http_conn->sender.ptr(),
                        mem,
                        end_of_request,
                        ret_accepted,
                        http_conn->cur_msg_data
                    /*)*/)
            || !res)
        {
            http_conn->cur_handler = NULL;
            *ret_accepted = mem.len();
        }
    }

    if (end_of_request)
        http_conn->cur_handler = NULL;
}

void
HttpService::doCloseHttpConnection (HttpConnection * const http_conn,
                                    HttpRequest    * const req)
{
//#warning I presume that HttpServer/Service guarantees synchronization domain.
//#warning But calling doCloseHttpConnection from the dtor violates the rule.
/* ^^^
   Решу это привязкой объектов к потокам. Простого решения нет.
   Очевидно, что в деструкторе нужно вызывать callback, который гарантирует
   контекст синхронизации. Единственный способ это сделать -
   вызывать callback из конкретного потока.
*/

    if (http_conn->cur_handler) {
        if (!http_conn->receiving_body) {
            bool dummy_block_input = false;
            HttpConnectionInfo conn_info (http_conn, &dummy_block_input);
            void *dummy_msg_data = NULL;
            http_conn->cur_handler->cb.call (
                    http_conn->cur_handler->cb->httpRequest,
                    /*(*/
                        req,
                        &conn_info,
                        http_conn->local_addr,
                        http_conn->sender.ptr(),
                        (http_conn->cur_handler->preassembly ?
                                Memory (http_conn->preassembly_buf,
                                        http_conn->preassembled_len)
                                : Memory()),
                        &dummy_msg_data
                    /*)*/);
            if (dummy_msg_data)
                logW_ (_func, "msg_data is likely lost");

            http_conn->receiving_body = true;
        }

        if (req && req->hasBody()) {
            bool dummy_block_input = false;
            HttpConnectionInfo conn_info (http_conn, &dummy_block_input);
            Size dummy_accepted = 0;
            http_conn->cur_handler->cb.call (
                    http_conn->cur_handler->cb->httpMessageBody,
                    /*(*/
                        req,
                        &conn_info,
                        http_conn->sender.ptr(),
                        Memory(),
                        true /* end_of_request */,
                        &dummy_accepted,
                        http_conn->cur_msg_data
                    /*)*/);
        }

	http_conn->cur_handler = NULL;
	logD (http_service, _func, "http_conn->cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);
    }

    Ref<HttpService> const self = http_conn->weak_http_service.getRef ();
    if (!self)
        return;

    self->destroyHttpConnection (http_conn);
}

void
HttpService::httpClosed (HttpRequest * const req,
                         Exception   * const exc_,
			 void        * const _http_conn)
{
    HttpConnection * const http_conn = static_cast <HttpConnection*> (_http_conn);

    logD_ (_func, "closed, http_conn 0x", fmt_hex, (UintPtr) http_conn);

    if (exc_)
	logE_ (_func, exc_->toString());

    logD (http_service, _func, "http_conn 0x", fmt_hex, (UintPtr) http_conn, ", "
          "refcount: ", fmt_def, http_conn->getRefCount(), ", "
          "cur_handler: 0x", fmt_hex, (UintPtr) http_conn->cur_handler);

    doCloseHttpConnection (http_conn, req);
}

bool
HttpService::acceptOneConnection ()
{
    HttpConnection * const http_conn = new (std::nothrow) HttpConnection (NULL /* embed_container */);
    assert (http_conn);

    IpAddress client_addr;
    {
	TcpServer::AcceptResult const res = tcp_server->accept (http_conn->tcp_conn,
                                                                &client_addr);
	if (res == TcpServer::AcceptResult::Error) {
	    http_conn->unref ();
	    logE_ (_func, exc->toString());
	    return false;
	}

	if (res == TcpServer::AcceptResult::NotAccepted) {
	    http_conn->unref ();
	    return false;
	}

	assert (res == TcpServer::AcceptResult::Accepted);
    }

    logD_ (_func, "accepted, http_conn 0x", fmt_hex, (UintPtr) http_conn, " client ", fmt_def, client_addr);

    http_conn->weak_http_service = this;

    if (http_conn->tcp_conn->getLocalAddress (&http_conn->local_addr)) {
        addr_saver.addAddress (http_conn->local_addr);
    } else {
        logF_ (_func, "tcp_conn.getLocalAddress() failed: ", exc->toString());
        if (!setIpAddress ("127.0.0.1", 0, &http_conn->local_addr))
            logF_ (_func, "setIpAddress(127.0.0.1) failed");
    }

    http_conn->cur_handler = NULL;
    http_conn->cur_msg_data = NULL;
    http_conn->receiving_body = false;
    http_conn->preassembly_buf = NULL;
    http_conn->preassembly_buf_size = 0;
    http_conn->preassembled_len = 0;

    http_conn->sender->init (deferred_processor);
    http_conn->sender->setConnection (http_conn->tcp_conn);
    http_conn->receiver->init (http_conn->tcp_conn,
                               deferred_processor);

    http_conn->http_server->init (
            CbDesc<HttpServer::Frontend> (&http_frontend, http_conn, http_conn),
            http_conn->receiver,
            http_conn->sender,
            deferred_processor,
            page_pool,
            client_addr);

    mutex.lock ();
    http_conn->pollable_key = poll_group->addPollable (http_conn->tcp_conn->getPollable());
    if (!http_conn->pollable_key) {
	mutex.unlock ();

	http_conn->unref ();

	logE_ (_func, exc->toString());
	return true;
    }

    if (keepalive_timeout_microsec > 0) {
//#warning TODO Proper HTTP timeouts - ? (Impact on HLS - ?)
	// TODO There should be a periodical checker routine which would
	// monitor connection's activity. Currently, this is an overly
	// simplistic oneshot cutter, like a ticking bomb for every client.
	http_conn->conn_keepalive_timer =
                timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (connKeepaliveTimerExpired,
                                                                              http_conn,
                                                                              http_conn),
                                               keepalive_timeout_microsec,
                                               false /* periodical */,
                                               false /* auto_delete */);
    }

    conn_list.append (http_conn);
    mutex.unlock ();

    http_conn->receiver->start ();
    return true;
}

TcpServer::Frontend const HttpService::tcp_server_frontend = {
    accepted
};

void
HttpService::accepted (void *_self)
{
    HttpService * const self = static_cast <HttpService*> (_self);

    logD (http_service, _func_);

    for (;;) {
	if (!self->acceptOneConnection ())
	    break;
    }
}

void
HttpService::addHttpHandler (CbDesc<HttpHandler> const &cb,
			     ConstMemory const path,
			     bool              preassembly,
			     Size        const preassembly_limit,
			     bool        const parse_body_params)
{
    if (preassembly_limit == 0)
        preassembly = false;

    mutex.lock ();

    NamespaceContainer<HandlerEntry>::Entry * const entry = namespace_container.addEntry (path);
    HandlerEntry * const handler_entry = &entry->data;

    handler_entry->cb = cb;
    handler_entry->preassembly = preassembly;
    handler_entry->preassembly_limit = preassembly_limit;
    handler_entry->parse_body_params = parse_body_params;

    mutex.unlock ();
}

mt_throws Result
HttpService::bind (IpAddress const &addr)
{
    if (!tcp_server->bind (addr))
	return Result::Failure;

    return Result::Success;
}

mt_throws Result
HttpService::start ()
{
    if (!tcp_server->listen ())
	return Result::Failure;

    // TODO Remove pollable when done.
    if (!poll_group->addPollable (tcp_server->getPollable()))
	return Result::Failure;

    if (!tcp_server->start ()) {
        logF_ (_func, "tcp_server.start() failed: ", exc->toString());
        return Result::Failure;
    }

    return Result::Success;
}

void
HttpService::setConfigParams (Time const keepalive_timeout_microsec,
                              bool const no_keepalive_conns)
{
    mutex.lock ();
    this->keepalive_timeout_microsec = keepalive_timeout_microsec;
    this->no_keepalive_conns = no_keepalive_conns;
    mutex.unlock ();
}

mt_throws Result
HttpService::init (PollGroup         * const mt_nonnull poll_group,
                   Timers            * const mt_nonnull timers,
                   DeferredProcessor * const mt_nonnull deferred_processor,
                   PagePool          * const mt_nonnull page_pool,
                   Time                const keepalive_timeout_microsec,
                   bool                const no_keepalive_conns,
                   unsigned            const max_saved_addrs,
                   unsigned            const max_saved_ports)
{
    addr_saver.init (max_saved_addrs, max_saved_ports);

    this->poll_group         = poll_group;
    this->timers             = timers;
    this->deferred_processor = deferred_processor;
    this->page_pool          = page_pool;

    this->keepalive_timeout_microsec = keepalive_timeout_microsec;
    this->no_keepalive_conns = no_keepalive_conns;

    if (!tcp_server->open ())
	return Result::Failure;

    tcp_server->init (CbDesc<TcpServer::Frontend> (&tcp_server_frontend, this, this),
                      deferred_processor,
                      timers);

    return Result::Success;
}

HttpService::HttpService (EmbedContainer * const embed_container)
    : Object                     (embed_container),
      keepalive_timeout_microsec (0),
      no_keepalive_conns         (false),
      tcp_server                 (this /* embed_container */)
{
}

HttpService::~HttpService ()
{
    mutex.lock ();

  // TODO Call remaining messageBody() callbacks to release callers' resources.

    ConnectionList::iter iter (conn_list);
    while (!conn_list.iter_done (iter)) {
	HttpConnection * const http_conn = conn_list.iter_next (iter);
	releaseHttpConnection (http_conn);
	http_conn->unref ();
    }

    mutex.unlock ();
}

}

