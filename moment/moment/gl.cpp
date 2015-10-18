/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/libmary.h>

#include <moment/moment_server.h>


#define MOMENT_GLC__SESSION_TIMEOUT_SEC (600)

#define MOMENT_GLC__INITIAL_T_MIN (  3 * 60)
#define MOMENT_GLC__INITIAL_T_MAX ( 15 * 60)

#define MOMENT_GLC__ERROR_T_MIN   ( 15 * 60)
#define MOMENT_GLC__ERROR_T_MAX   ( 60 * 60)

#define MOMENT_GLC__NEXT_T_MIN    ( 60 * 60)
#define MOMENT_GLC__NEXT_T_MAX    (300 * 60)


namespace Moment {

using namespace M;

extern StRef<String> primak;

class Glc : public Object
{
private:
    StateMutex mutex;

    enum Attempt {
        Attempt_Http,
        Attempt_Tcp,
        Attempt_Udp
    };

    class Session : public Object
    {
    public:
        mt_const WeakRef<Glc> weak_gl_client;

        Session (EmbedContainer * const embed_container)
            : Object (embed_container)
        {}
    };

    class Session_Http : public Session
    {
    public:
        Embed<HttpClient> http_client;

        Session_Http (EmbedContainer * const embed_container)
            : Session     (embed_container),
              http_client (this /* embed_container */)
        {}
    };

    class Session_Tcp : public Session
    {
    public:
        Embed<TcpConnection>             tcp_conn;
        Embed<ImmediateConnectionSender> sender;
        Embed<AsyncStreamReceiver>       receiver;
        Embed<MessageServer>             message_server;

        Session_Tcp (EmbedContainer * const embed_container)
            : Session        (embed_container),
              tcp_conn       (this /* embed_container */),
              sender         (this /* embed_container */),
              receiver       (this /* embed_container */),
              message_server (this /* embed_container */)
        {}
    };

    class Session_Udp : public Session
    {
    public:
        Embed<UdpSocket>                udp_socket;
        Embed<DeferredConnectionSender> sender;

        Session_Udp (EmbedContainer * const embed_container)
            : Session    (embed_container),
              udp_socket (this /* embed_container */),
              sender     (this /* embed_container */, true /* datagram_mode */)
        {}
    };

    mt_const Ref<ServerContext> server_ctx;
    mt_const Ref<PagePool> page_pool;

    mt_const IpAddress gl_addr;

    mt_mutex (mutex) Attempt attempt;
    mt_mutex (mutex) Ref<Session> session;
    // TODO Make use of clearance_unixtime.
    mt_mutex (mutex) Time clearance_unixtime;

    void glSetTimer (Uint32 t_min,
                     Uint32 t_max);

    static void glTimerTick (void *_self);

    static void sessionTimeout (void *_session);

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderClosed (Exception *exc_,
                              void      *_session);
  mt_iface_end

  mt_iface (HttpClient::HttpResponseHandler)
    static HttpClient::HttpResponseHandler const http_response_handler;

    static Result httpResponse (HttpRequest  *resp,
                                Memory        msg_body,
                                void        ** mt_nonnull ret_msg_data,
                                void         *_session_http);
  mt_iface_end

  mt_iface (TcpConnection::Frontend)
    static TcpConnection::Frontend const tcp_conn_frontend;

    static void tcpConnected (Exception *exc_,
                              void      *_session_tcp);
  mt_iface_end

    void doTcp_afterConnect (Session_Tcp * mt_nonnull session_tcp);

  mt_iface (MessageServer::Frontend)
    static MessageServer::Frontend const message_frontend;

    static void msg_message (Memory  msg,
                             void   *_session_tcp);

    static void msg_closed (void *_session_tcp);
  mt_iface_end

    Result doHttp ();
    Result doTcp  ();
    Result doUdp  ();

    void doGl ();

    mt_unlocks (mutex) void processResponse (Memory mem);

public:
    mt_const void init (ServerContext *server_ctx,
                        PagePool      *page_pool);

    Glc (EmbedContainer *embed_container);
};

void
Glc::glSetTimer (Uint32 const t_min,
                 Uint32 const t_max)
{
    Uint32 const t = randomUint32() % (t_max - t_min) + t_min;

    server_ctx->getMainThreadContext()->getTimers()->addTimer_microseconds (
            CbDesc<Timers::TimerCallback> (glTimerTick, this, this),
            (Uint64) t * 1000000,
            false /* periodical */,
            true  /* auto_delete */,
            true  /* delete_after_tick */);
}

void
Glc::glTimerTick (void * const _self)
{
    Glc * const self = static_cast <Glc*> (_self);

    self->doGl ();
}

void
Glc::sessionTimeout (void * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<Glc> const self = session->weak_gl_client.getRef();
    if (!self)
        return;

    self->mutex.lock ();
    if (self->session != session) {
        self->mutex.unlock ();
        return;
    }
    self->session = NULL;
    self->mutex.unlock ();

    self->glSetTimer (MOMENT_GLC__ERROR_T_MIN,
                      MOMENT_GLC__ERROR_T_MAX);
}

Sender::Frontend const Glc::sender_frontend = {
    NULL /* sendStateChanged */,
    senderClosed
};

void
Glc::senderClosed (Exception * const /* exc_ */,
                   void      * const _session)
{
    Session * const session = static_cast <Session*> (_session);
    Ref<Glc> const self = session->weak_gl_client.getRef();
    if (!self)
        return;

    self->mutex.lock ();
    if (self->session.ptr() != session) {
        self->mutex.unlock ();
        return;
    }
    self->session = NULL;
    self->mutex.unlock ();

    self->glSetTimer (MOMENT_GLC__ERROR_T_MIN,
                      MOMENT_GLC__ERROR_T_MAX);
}

HttpClient::HttpResponseHandler const Glc::http_response_handler = {
    httpResponse,
    NULL /* httpResponseBody */
};

Result
Glc::httpResponse (HttpRequest  * const /* resp */,
                   Memory         const msg_body,
                   void        ** const mt_nonnull /* ret_msg_data */,
                   void         * const _session_http)
{
    Session_Http * const session_http = static_cast <Session_Http*> (_session_http);
    Ref<Glc> const self = session_http->weak_gl_client.getRef();
    if (!self)
        return Result::Failure;

    self->mutex.lock ();
    if (self->session.ptr() != session_http) {
        self->mutex.unlock ();
        return Result::Failure;
    }

    mt_unlocks (mutex) self->processResponse (msg_body);

    return Result::Success;
}

Result
Glc::doHttp ()
{
    Ref<Session_Http> const session_http = grabNewObject <Session_Http> ();
    session_http->weak_gl_client = this;
    session_http->http_client->init (server_ctx->selectThreadContext(),
                                     page_pool,
                                     IpAddress()   /* server_addr */,
                                     "stat.momentvideo.org" /* host */,
                                     false         /* keepalive */,
                                     4096          /* preassembly_limit */);

    mutex.lock ();
    session = session_http;
    attempt = Attempt_Tcp;
    mutex.unlock ();

    if (!session_http->http_client->httpPost (ConstMemory() /* req_path */,
                                              ConstMemory() /* post_data */,
                                              CbDesc<HttpClient::HttpResponseHandler> (&http_response_handler, this, this),
                                              true          /* preassembly */,
                                              false         /* parse_body_params */,
                                              false         /* use_http_1_0 */))
    {
        return Result::Failure;
    }

    server_ctx->getMainThreadContext()->getTimers()->addTimer_microseconds (
            CbDesc<Timers::TimerCallback> (sessionTimeout,
                                           static_cast <Session*> (session_http),
                                           static_cast <Session*> (session_http)),
            MOMENT_GLC__SESSION_TIMEOUT_SEC * 1000000,
            false /* periodical */,
            true  /* auto_delete */,
            true  /* delete_after_tick */);

    return Result::Success;
}

TcpConnection::Frontend const Glc::tcp_conn_frontend = {
    tcpConnected
};

void
Glc::tcpConnected (Exception * const exc_,
                   void      * const _session_tcp)
{
    Session_Tcp * const session_tcp = static_cast <Session_Tcp*> (_session_tcp);
    Ref<Glc> const self = session_tcp->weak_gl_client.getRef();
    if (!self)
        return;

    self->mutex.lock ();
    if (self->session.ptr() != session_tcp) {
        self->mutex.unlock ();
        return;
    }

    if (exc_) {
        self->session = NULL;
        self->mutex.unlock ();

        self->glSetTimer (MOMENT_GLC__ERROR_T_MIN,
                          MOMENT_GLC__ERROR_T_MAX);
        return;
    }

    self->doTcp_afterConnect (session_tcp);
}

void
Glc::doTcp_afterConnect (Session_Tcp * const mt_nonnull session_tcp)
{
    session_tcp->receiver->start ();

    session_tcp->sender->send (page_pool,
                               true /* do_flush */,
                               "Hello, World!\n");
}

MessageServer::Frontend const Glc::message_frontend = {
    msg_message,
    msg_closed
};

void
Glc::msg_message (Memory   const msg,
                  void   * const _session_tcp)
{
    Session_Tcp * const session_tcp = static_cast <Session_Tcp*> (_session_tcp);
    Ref<Glc> const self = session_tcp->weak_gl_client.getRef();
    if (!self)
        return;

    self->mutex.lock ();
    if (self->session.ptr() != session_tcp) {
        self->mutex.unlock ();
        return;
    }

    mt_unlocks (mutex) self->processResponse (msg);
}

void
Glc::msg_closed (void * const _session_tcp)
{
    Session_Tcp * const session_tcp = static_cast <Session_Tcp*> (_session_tcp);
    Ref<Glc> const self = session_tcp->weak_gl_client.getRef();
    if (!self)
        return;

    self->mutex.lock ();
    if (self->session.ptr() != session_tcp) {
        self->mutex.unlock ();
        return;
    }
    self->session = NULL;
    self->mutex.unlock ();

    self->glSetTimer (MOMENT_GLC__ERROR_T_MIN,
                      MOMENT_GLC__ERROR_T_MAX);
}

Result
Glc::doTcp ()
{
    Ref<Session_Tcp> const session_tcp = grabNewObject <Session_Tcp> ();
    session->weak_gl_client = this;

    session_tcp->tcp_conn->setFrontend (CbDesc<TcpConnection::Frontend> (&tcp_conn_frontend, session_tcp, session_tcp));

    session_tcp->sender->init (server_ctx->getMainThreadContext()->getDeferredProcessor());
    session_tcp->sender->setFrontend (
            CbDesc<Sender::Frontend> (&sender_frontend,
                                      static_cast <Session*> (session_tcp),
                                      static_cast <Session*> (session_tcp)));
    session_tcp->sender->setConnection (session_tcp->tcp_conn);

    session_tcp->receiver->init (session_tcp->tcp_conn,
                                 server_ctx->getMainThreadContext()->getDeferredProcessor());

    session_tcp->message_server->init (session_tcp->receiver,
                                       CbDesc<MessageServer::Frontend> (&message_frontend, session_tcp, session_tcp),
                                       4096 /* max_msg_len */);

    mutex.lock ();
    session = session_tcp;
    attempt = Attempt_Udp;
    mutex.unlock ();

    if (!session_tcp->tcp_conn->open ())
        return Result::Failure;

    if (!server_ctx->getMainThreadContext()->getPollGroup()->addPollable_beforeConnect (
                session_tcp->tcp_conn->getPollable(), NULL /* ret_key */, true /* auto_remove */))
    {
        return Result::Failure;
    }

    TcpConnection::ConnectResult const connect_res = session_tcp->tcp_conn->connect (gl_addr);
    if (connect_res == TcpConnection::ConnectResult_Error)
        return Result::Failure;

    if (!server_ctx->getMainThreadContext()->getPollGroup()->addPollable_afterConnect (
                session_tcp->tcp_conn->getPollable(), NULL /* ret_key */, true /* auto_remove */))
    {
        return Result::Failure;
    }

    server_ctx->getMainThreadContext()->getTimers()->addTimer_microseconds (
            CbDesc<Timers::TimerCallback> (sessionTimeout,
                                           static_cast <Session*> (session_tcp),
                                           static_cast <Session*> (session_tcp)),
            MOMENT_GLC__SESSION_TIMEOUT_SEC * 1000000,
            false /* periodical */,
            true  /* auto_delete */,
            true  /* delete_after_tick */);

    if (connect_res == TcpConnection::ConnectResult_Connected) {
        doTcp_afterConnect (session_tcp);
        return Result::Success;
    }

    return Result::Success;
}

Result
Glc::doUdp ()
{
    Ref<Session_Udp> const session_udp = grabNewObject <Session_Udp> ();
    session_udp->weak_gl_client = this;

    if (!session_udp->udp_socket->open ())
        return Result::Failure;

    session_udp->sender->setConnection (session_udp->udp_socket);
    session_udp->sender->setQueue (server_ctx->getMainThreadContext()->getDeferredConnectionSenderQueue());

    session_udp->sender->setDatagramSendtoAddress (gl_addr);
    // We set glSetTimer() immediately here => we should not handle errors.
    // session_udp->sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, session_udp, session_udp));

    server_ctx->getMainThreadContext()->getPollGroup()->addPollable (
            session_udp->udp_socket->getPollable(), true /* auto_remove */);

    session_udp->sender->send (page_pool, true /* do_flush */, "Hello, World!\n");

    mutex.lock ();
    // It is important not to subscribe for any session_udp events, because
    // we call glSetTimer() and keep non-null 'session' at the same time.
    session = session_udp;
    attempt = Attempt_Http;
    mutex.unlock ();

    glSetTimer (MOMENT_GLC__NEXT_T_MIN,
                MOMENT_GLC__NEXT_T_MAX);

    return Result::Success;
}

void
Glc::doGl ()
{
    session = NULL;

    switch (attempt) {
        case Attempt_Http: {
            if (doHttp ())
                return;
        } break;
        case Attempt_Tcp: {
            if (doTcp ())
                return;
        } break;
        case Attempt_Udp: {
            if (doUdp ())
                return;
        } break;
    }

    glSetTimer (MOMENT_GLC__ERROR_T_MIN,
                MOMENT_GLC__ERROR_T_MAX);
}

mt_unlocks (mutex) void
Glc::processResponse (Memory const mem)
{
//#warning TEST
    logD_ (_func, ":");
    hexdump (mem);

    session = NULL;
    mutex.unlock ();

    glSetTimer (MOMENT_GLC__NEXT_T_MIN,
                MOMENT_GLC__NEXT_T_MAX);
}

mt_const void
Glc::init (ServerContext * const server_ctx,
           PagePool      * const page_pool)
{
    this->server_ctx = server_ctx;
    this->page_pool = page_pool;

    if (!setIpAddress ("localhost:5555", &gl_addr))
        return;

    glSetTimer (MOMENT_GLC__INITIAL_T_MIN,
                MOMENT_GLC__INITIAL_T_MAX);
}

Glc::Glc (EmbedContainer * const embed_container)
    : Object              (embed_container),
      attempt             (Attempt_Http),
      clearance_unixtime  (0)
{
}

void
initGl ()
{
    MomentServer * const moment = MomentServer::getInstance();

    Glc * const glc = new (std::nothrow) Glc (NULL /* embed_container */);
    assert (glc);

    glc->init (moment->getServerApp()->getServerContext(),
               moment->getPagePool());
}

}

