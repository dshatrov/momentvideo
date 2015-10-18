/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/http_client.h>
#include <libmary/util_dev.h>


namespace M {

static LogGroup libMary_logGroup_http_client ("http_client", LogLevel::I);

HttpClient::HttpClientConnection::HttpClientConnection (EmbedContainer * const embed_container)
    : Object (embed_container),
      tcp_conn    (this /* embed_container */),
      sender      (this /* embed_container */),
      receiver    (this /* embed_container */),
      http_server (this /* embed_container */)
{
}

HttpClient::HttpClientConnection::~HttpClientConnection ()
{
    delete[] preassembly_buf;
}

TcpConnection::Frontend const HttpClient::tcp_conn_frontend = {
    connected
};

void
HttpClient::connected (Exception * const exc_,
                       void      * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    Ref<HttpClient> const self = http_conn->weak_http_client.getRef ();
    if (!self)
        return;

    logD (http_client, _func_);

    self->mutex.lock ();

    if (exc_) {
        self->destroyHttpClientConnection (http_conn, NULL /* reply */);
        self->mutex.unlock ();
        return;
    }

    http_conn->connected = true;

    List< Ref<HttpClientRequest> >::iter iter (http_conn->requests);
    while (!http_conn->requests.iter_done (iter)) {
        Ref<HttpClientRequest> &http_req = http_conn->requests.iter_next (iter)->data;
        self->sendRequest (http_conn, http_req);
    }

    self->mutex.unlock ();

    http_conn->receiver->start ();
}

Sender::Frontend const HttpClient::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
HttpClient::senderStateChanged (SenderState   const /* sender_state */,
                                void        * const /* _http_conn */)
{
//    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
//    HttpClient * const self = http_conn->http_client;

    logD (http_client, _func_);

/*
// TODO This will be covered by reply timeouts.
//      ^^^ Reacting to QueueHardLimit is still a good idea.

    if (sender_state == SenderState::QueueHardLimit) {
        self->mutex.lock ();
        mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn);
        self->mutex.unlock ();
    }
*/
}

void
HttpClient::senderClosed (Exception * const exc_,
                          void      * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    Ref<HttpClient> const self = http_conn->weak_http_client.getRef ();
    if (!self)
        return;

    if (exc_)
        logE (http_client, _func, "exception: ", exc_->toString());

    self->mutex.lock ();
    mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn, NULL /* reply */);
    self->mutex.unlock ();
}

HttpServer::Frontend const HttpClient::http_server_frontend = {
    NULL /* rawData */,
    httpReply,
    httpReplyBody,
    httpClosed
};

void
HttpClient::httpReply (HttpRequest * const mt_nonnull reply,
                       bool        * const mt_nonnull /* ret_block_input */,
                       void        * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    Ref<HttpClient> const self = http_conn->weak_http_client.getRef ();
    if (!self)
        return;

    logD (http_client, _self_func_);

    self->mutex.lock ();
    if (!http_conn->valid) {
        logD (http_client, _func, "http_conn gone");
        self->mutex.unlock ();
        return;
    }

    if (http_conn->requests.isEmpty()) {
        logE (http_client, _func, "spurious HTTP reply, disconnecting");
        self->destroyHttpClientConnection (http_conn, NULL /* reply */);
        self->mutex.unlock ();
        return;
    }

    Ref<HttpClientRequest> const http_req = http_conn->requests.getFirst();
    if (!reply->hasBody()) {
        // Note that we remove the current request from the list so that early
        // destroyHttpClientConnection() won't call any callbacks for it.
        http_conn->requests.remove (http_req->req_list_el);
        http_req->req_list_el = NULL;

        // TODO Deal with non-keepalive responses to keepalive requests.
        if (!self->keepalive) {
            mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn, reply);
            // 'http_conn' is still accessible (referenced) and preassembly state
            // is kept valid.
        }
    }

    self->mutex.unlock ();

    // Note: There's no way to set 'discarded' to true before we get here,
    //       so 'discarded' is always false at this point.
    if (http_req->discarded || !http_req->response_cb)
        return;

    http_conn->preassembled_len = 0;

    if (!http_req->preassembly || !reply->hasBody()) {
        if (http_req->response_cb && http_req->response_cb->httpResponse) {
            Result res = Result::Failure;
            if (!http_req->response_cb.call_ret<Result> (&res,
                                                         http_req->response_cb->httpResponse,
                                                         /*(*/
                                                             reply,
                                                             Memory(),
                                                             &http_req->user_msg_data
                                                         /*)*/)
                || !res)
            {
                http_req->discarded = true;
            }

            if (http_req->user_msg_data && !reply->hasBody())
                logW (http_client, _func, "msg_data is likely lost");

            http_req->receiving_body = true;
        }
    }
}

// Request body assembly logics is very similar to that in HttpService,
// almost to the point of code duplication. It probably belongs to HttpServer.
void
HttpClient::httpReplyBody (HttpRequest  * const mt_nonnull reply,
                           Memory         const mem,
                           bool           const end_of_reply,
                           Size         * const mt_nonnull ret_accepted,
                           bool         * const mt_nonnull /* ret_block_input */,
                           void         * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    Ref<HttpClient> const self = http_conn->weak_http_client.getRef ();
    if (!self)
        return;

    logD (http_client, _self_func_, "mem.len(): ", mem.len(), ", end_of_reply: ", end_of_reply);

    self->mutex.lock ();
    if (!http_conn->valid) {
        logD (http_client, _func, "http_conn gone");
        *ret_accepted = mem.len();
        self->mutex.unlock ();
        return;
    }

    if (http_conn->requests.isEmpty()) {
        logE (http_client, _func, "spurious HTTP reply, disconnecting");
        self->destroyHttpClientConnection (http_conn, NULL /* reply */);
        self->mutex.unlock ();
        *ret_accepted = mem.len();
        return;
    }

    Ref<HttpClientRequest> const http_req = http_conn->requests.getFirst();
    if (end_of_reply) {
        // Note that we remove the current request from the list so that early
        // destroyHttpClientConnection() won't call any callbacks for it.
        http_conn->requests.remove (http_req->req_list_el);
        http_req->req_list_el = NULL;

        if (!self->keepalive) {
            mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn, NULL /* reply */);
            // 'http_conn' is still accessible (referenced) and preassembly state
            // is kept valid.
        }
    }

    self->mutex.unlock ();

    if (http_req->discarded || !http_req->response_cb) {
        *ret_accepted = mem.len();
        return;
    }

    if (http_req->preassembly
        && http_conn->preassembled_len < self->preassembly_limit)
    {
        // 'size' is how much we're going to preassemble for this reply.
	Size size = self->preassembly_limit;
        if (reply->getContentLengthSpecified() && reply->getContentLength() < size)
            size = reply->getContentLength();

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
            || end_of_reply)
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

            if (http_req->parse_body_params) {
                reply->parseParameters (
                        Memory (http_conn->preassembly_buf,
                                http_conn->preassembled_len));
            }

            if (http_req->response_cb && http_req->response_cb->httpResponse) {
                Result res = Result::Failure;
                if (!http_req->response_cb.call_ret<Result> (
                            &res,
                            http_req->response_cb->httpResponse,
                            /*(*/
                                reply,
                                Memory (http_conn->preassembly_buf,
                                        http_conn->preassembled_len),
                                &http_req->user_msg_data
                            /*)*/)
                    || !res)
                {
                    http_req->discarded = true;
                }
            }
            http_req->receiving_body = true;

            if (!http_req->discarded) {
                if (*ret_accepted < mem.len()) {
                    Size accepted = 0;
                    Result res = Result::Failure;
                    if (!http_req->response_cb.call_ret<Result> (
                                &res,
                                http_req->response_cb->httpResponseBody,
                                /*(*/
                                    reply,
                                    mem.region (*ret_accepted),
                                    end_of_reply,
                                    &accepted,
                                    http_req->user_msg_data
                                /*)*/)
                        || !res)
                    {
                        http_req->discarded = true;
                    }

                    *ret_accepted += accepted;
                } else {
                    if (end_of_reply && !reply->getContentLengthSpecified()) {
                        Size dummy_accepted = 0;
                        Result res = Result::Failure;
                        if (!http_req->response_cb.call_ret<Result> (
                                    &res,
                                    http_req->response_cb->httpResponseBody,
                                    /*(*/
                                        reply,
                                        Memory(),
                                        end_of_reply,
                                        &dummy_accepted,
                                        http_req->user_msg_data
                                    /*)*/)
                            || !res)
                        {
                            http_req->discarded = true;
                        }
                    }
                }
            }

            if (http_req->discarded)
                *ret_accepted = mem.len();

            if (end_of_reply)
                http_req->discarded = true;
        } else {
            memcpy (http_conn->preassembly_buf + http_conn->preassembled_len,
                    mem.mem(),
                    mem.len());
            *ret_accepted = mem.len();
            http_conn->preassembled_len += mem.len();
        }

        return;
    }

    Result res = Result::Failure;
    if (!http_req->response_cb.call_ret<Result> (
                &res,
                http_req->response_cb->httpResponseBody,
                /*(*/
                    reply,
                    mem,
                    end_of_reply,
                    ret_accepted,
                    http_req->user_msg_data
                /*)*/)
        || !res)
    {
        http_req->discarded = true;
        *ret_accepted = mem.len();
    }

    if (end_of_reply)
        http_req->discarded = true;
}

void
HttpClient::httpClosed (HttpRequest * const reply,
                        Exception   * const exc_,
                        void        * const _http_conn)
{
    HttpClientConnection * const http_conn = static_cast <HttpClientConnection*> (_http_conn);
    Ref<HttpClient> const self = http_conn->weak_http_client.getRef ();
    if (!self)
        return;

    logD (http_client, _func_);

    if (exc_)
        logE (http_client, _func, "exception: ", exc_->toString());

    self->mutex.lock ();
    mt_unlocks_locks (mutex) self->destroyHttpClientConnection (http_conn, reply);
    self->mutex.unlock ();
}

mt_unlocks_locks (mutex) void
HttpClient::destroyHttpClientConnection (HttpClientConnection * const mt_nonnull _http_conn,
                                         HttpRequest          *reply)
{
    if (!_http_conn->valid) {
        return;
    }
    _http_conn->valid = false;

    Ref<HttpClientConnection> const http_conn = _http_conn;
    if (http_conn->conn_list_el) {
        http_conns.remove (http_conn->conn_list_el);
        http_conn->conn_list_el = NULL;
    }

    Ref<ServerThreadContext> const thread_ctx = http_conn->weak_thread_ctx.getRef ();
    if (thread_ctx) {
        if (http_conn->pollable_key) {
            thread_ctx->getPollGroup()->removePollable (http_conn->pollable_key);
            http_conn->pollable_key = NULL;
        }

// TODO Explicit closing is useful but leads to race conditions in class TcpConnection.
//        if (!http_conn->tcp_conn.close ())
//            logE (http_client, _func, "tcp_conn.close() failed: ", exc->toString());

      // Manually discarding sender and receiver buffers here might be a good idea.
    }

    {
        List< Ref<HttpClientRequest> >::iter iter (http_conn->requests);
        while (!http_conn->requests.iter_done (iter)) {
            Ref<HttpClientRequest> &http_req = http_conn->requests.iter_next (iter)->data;

//#warning 'discarded' and 'receiving_body' are not synchronized properly when this func is called from dtor.
//#warning Bind HttpClientConnection to its thread to resolve this.
            if (!http_req->discarded && http_req->response_cb) {
                if (!http_req->receiving_body) {
                    void *dummy_msg_data = NULL;
                    mt_unlocks_locks (mutex) http_req->response_cb.call_mutex (
                            http_req->response_cb->httpResponse,
                            mutex,
                            /*(*/
                                reply,
                                (http_req->preassembly ?
                                          Memory (http_conn->preassembly_buf,
                                                  http_conn->preassembled_len)
                                        : Memory()),
                                &dummy_msg_data
                            /*)*/);
                    if (dummy_msg_data)
                        logW (http_client, _func, "msg_data is likely lost");

                    http_req->receiving_body = true;
                }

                if (reply && reply->hasBody()) {
                    Size dummy_accepted = 0;
                    mt_unlocks_locks (mutex) http_req->response_cb.call_mutex (
                            http_req->response_cb->httpResponseBody,
                            mutex,
                            /*(*/
                                reply,
                                Memory(),
                                true /* end_of_response */,
                                &dummy_accepted,
                                http_req->user_msg_data
                            /*)*/);
                }
            }

            http_conn->requests.remove (http_req->req_list_el);

            reply = NULL;
        }
        assert (http_conn->requests.isEmpty());
    }
}

mt_mutex (mutex) Ref<HttpClient::HttpClientConnection>
HttpClient::connect (bool * const ret_connected)
{
    *ret_connected = false;

    Ref<HttpClientConnection> const http_conn = grabNewObject <HttpClientConnection> ();
    http_conn->weak_http_client = this;
    http_conn->server_addr = next_server_addr;
    http_conn->valid = true;
    http_conn->connected = false;
    http_conn->conn_list_el = NULL;

    http_conn->preassembly_buf = NULL;
    http_conn->preassembly_buf_size = 0;
    http_conn->preassembled_len = 0;

    http_conn->weak_thread_ctx = thread_ctx;

    http_conn->tcp_conn->setFrontend (
            CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, http_conn, http_conn));

    http_conn->sender->setConnection (http_conn->tcp_conn);
    http_conn->sender->setQueue (thread_ctx->getDeferredConnectionSenderQueue());
    http_conn->sender->setFrontend (
            CbDesc<Sender::Frontend> (&sender_frontend, http_conn, http_conn));

    http_conn->receiver->init (http_conn->tcp_conn,
                               thread_ctx->getDeferredProcessor());

    http_conn->http_server->init (
            CbDesc<HttpServer::Frontend> (&http_server_frontend, http_conn, http_conn),
            http_conn->receiver,
            NULL /* sender */,
            NULL /* deferred_processor */,
            NULL /* page_pool */,
            IpAddress(),
            true /* client_mode */);

    if (!http_conn->tcp_conn->open ()) {
        logE (http_client, _this_func, "http_conn->open() failed: ", exc->toString());
        return NULL;
    }

    if (!thread_ctx->getPollGroup()->addPollable_beforeConnect (http_conn->tcp_conn->getPollable(),
                                                                &http_conn->pollable_key))
    {
        logE (http_client, _this_func, "addPollable_beforeConnect() failed: ", exc->toString());
        return NULL;
    }

    TcpConnection::ConnectResult const connect_res = http_conn->tcp_conn->connect (next_server_addr);
    if (connect_res == TcpConnection::ConnectResult_Error) {
        logE (http_client, _this_func, "http_conn->connect() failed: ", exc->toString());

        if (http_conn->pollable_key) {
            thread_ctx->getPollGroup()->removePollable (http_conn->pollable_key);
            http_conn->pollable_key = NULL;
        }

        return NULL;
    }

    if (!thread_ctx->getPollGroup()->addPollable_afterConnect (http_conn->tcp_conn->getPollable(),
                                                               &http_conn->pollable_key))
    {
        logE (http_client, _this_func, "addPollable_afterConnect() failed: ", exc->toString());
        return NULL;
    }

    if (connect_res == TcpConnection::ConnectResult_Connected) {
        http_conn->connected = true;
        *ret_connected = true;

        http_conn->receiver->start ();
    } else {
        logD (http_client, _this_func, "connection in progress");
        assert (connect_res == TcpConnection::ConnectResult_InProgress);
    }

    http_conn->conn_list_el = http_conns.append (http_conn);

    return http_conn;
}

mt_mutex (mutex) Ref<HttpClient::HttpClientConnection>
HttpClient::getConnection (bool * const ret_connected,
                           bool   const force_new_connection)
{
    *ret_connected = false;

    if (!force_new_connection
        && cur_server_addr == next_server_addr
        && keepalive
        && !http_conns.isEmpty())
    {
        Ref<HttpClientConnection> const client_conn = http_conns.getLast();
        *ret_connected = client_conn->connected;
        return client_conn;
    }

    cur_server_addr = next_server_addr;
    return connect (ret_connected);
}

mt_mutex (mutex) void
HttpClient::sendRequest (HttpClientConnection * const mt_nonnull http_conn,
                         HttpClientRequest    * const mt_nonnull http_req)
{
    // TODO keep-alive header for keepalive mode
    http_conn->sender->send (page_pool,
                             true /* do_flush */,
                             (http_req->req_type == HttpRequestType_Get ? "GET" : "POST"),
                             " ", http_req->req_path,
                             (http_req->use_http_1_0 ? " HTTP/1.0" : " HTTP/1.1"),
                             "\r\n"
                             "Host: ", host, "\r\n"
                             "Content-Length: ", http_req->post_data.mem.len(), "\r\n",
                             // TODO http_req->post_content_type
                             (http_req->post_data.mem.len() ? ("Content-Type: application/x-www-form-urlencoded\r\n") : ConstMemory()),
                             "\r\n",
                             http_req->post_data.mem);
}

Result
HttpClient::queueRequest (HttpRequestType const req_type,
                          ConstMemory     const req_path,
                          ConstMemory     const post_data,
                          CbDesc<HttpResponseHandler> const &response_cb,
                          bool                  preassembly,
                          bool            const parse_body_params,
                          bool            const use_http_1_0)
{
    if (preassembly_limit == 0)
        preassembly = false;

    mutex.lock ();

    bool connected = false;
    Ref<HttpClientConnection> const http_conn =
            getConnection (&connected, use_http_1_0 /* force_new_connection */);
    if (!http_conn) {
        mutex.unlock ();
        return Result::Failure;
    }

    Ref<HttpClientRequest> const http_req = grabNew <HttpClientRequest> ();
    http_req->req_type = req_type;
    http_req->req_path = newString (req_path);
    http_req->post_data.set (post_data);
    http_req->response_cb = response_cb;

    http_req->preassembly = preassembly;
    http_req->parse_body_params = parse_body_params;
    http_req->use_http_1_0 = use_http_1_0;

    http_req->receiving_body = false;
    http_req->user_msg_data = NULL;

    http_req->discarded = false;

    http_req->req_list_el = http_conn->requests.append (http_req);

    if (connected)
        sendRequest (http_conn, http_req);

    mutex.unlock ();

    return Result::Success;
}

Result
HttpClient::httpGet (ConstMemory const req_path,
                     CbDesc<HttpResponseHandler> const &response_cb,
                     bool        const preassembly,
                     bool        const parse_body_params,
                     bool        const use_http_1_0)
{
    return queueRequest (HttpRequestType_Get,
                         req_path,
                         ConstMemory() /* post_data */,
                         response_cb,
                         preassembly,
                         parse_body_params,
                         use_http_1_0);
}

Result
HttpClient::httpPost (ConstMemory const req_path,
                      ConstMemory const post_data,
                      CbDesc<HttpResponseHandler> const &response_cb,
                      bool        const preassembly,
                      bool        const parse_body_params,
                      bool        const use_http_1_0)
{
    return queueRequest (HttpRequestType_Post,
                         req_path,
                         post_data,
                         response_cb,
                         preassembly,
                         parse_body_params,
                         use_http_1_0);
}

void
HttpClient::setServerAddr (IpAddress   const server_addr,
                           ConstMemory const host)
{
    mutex.lock ();
    this->next_server_addr = server_addr;
    this->host = newString (host);
    mutex.unlock ();
}

mt_const void
HttpClient::init (ServerThreadContext * const mt_nonnull thread_ctx,
                  PagePool            * const mt_nonnull page_pool,
                  IpAddress             const server_addr,
                  ConstMemory           const host,
                  bool                  const keepalive,
                  Size                  const preassembly_limit)
{
    this->keepalive         = keepalive;
    this->thread_ctx        = thread_ctx;
    this->page_pool         = page_pool;
    next_server_addr        = server_addr;
    this->host              = newString (host);
    this->preassembly_limit = preassembly_limit;
}

HttpClient::HttpClient (EmbedContainer * const embed_container)
    : Object    (embed_container),
      keepalive (false)
{
}

HttpClient::~HttpClient ()
{
    mutex.lock ();

    List< Ref<HttpClientConnection> >::iter iter (http_conns);
    while (!http_conns.iter_done (iter)) {
        Ref<HttpClientConnection> &http_conn = http_conns.iter_next (iter)->data;
        mt_unlocks_locks (mutex) destroyHttpClientConnection (http_conn, NULL /* reply */);
    }

    mutex.unlock ();
}

}

