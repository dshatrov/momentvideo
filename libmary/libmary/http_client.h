/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__HTTP_CLIENT__H__
#define LIBMARY__HTTP_CLIENT__H__


#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/tcp_connection.linux.h>
  #include <libmary/connection_receiver.generic.h>
#else
  #include <libmary/tcp_connection.h>
  #include <libmary/connection_receiver.h>
#endif
#include <libmary/deferred_connection_sender.h>
#include <libmary/http_server.h>
#include <libmary/server_context.h>


namespace M {

class HttpClient : public Object
{
  private:
    StateMutex mutex;

  public:
    struct HttpResponseHandler
    {
        // If the module has subscribed to request message body pre-assembly,
        // then @msg_body points to message body.
        //
        // If @resp is NULL, then there was an error (no response).
        Result (*httpResponse) (HttpRequest   *resp,
                                Memory         msg_body,
                                void         ** mt_nonnull ret_msg_data,
                                void          *cb_data);

        // If @resp is NULL, then we have not received the response in full due to
        // an error. Such last call is made to give the module a chance
        // to release msg_data.
        Result (*httpResponseBody) (HttpRequest  *resp,
                                    Memory        mem,
                                    bool          end_of_response,
                                    Size         * mt_nonnull ret_accepted,
                                    void         *msg_data,
                                    void         *cb_data);
    };

  private:
    enum HttpRequestType
    {
        HttpRequestType_Get,
        HttpRequestType_Post
    };

    class HttpClientRequest : public Referenced
    {
      public:
        mt_const HttpRequestType req_type;
        mt_const StRef<String> req_path;
        mt_const Buffer post_data;
        mt_const Cb<HttpResponseHandler> response_cb;

        mt_const bool preassembly;
        mt_const bool parse_body_params;
        mt_const bool use_http_1_0;

        mt_mutex (HttpClient::Mutex) bool receiving_body;
        mt_mutex (HttpClient::Mutex) List< Ref<HttpClientRequest> >::Element *req_list_el;

        mt_mutex (HttpClient::Mutex) void *user_msg_data;

        mt_sync (HttpClientConnection::http_server)
        mt_begin
          bool discarded;
        mt_end
    };

    class HttpClientConnection : public Object
    {
      public:
        mt_const WeakRef<HttpClient> weak_http_client;

        mt_const IpAddress server_addr;

        mt_const WeakRef<ServerThreadContext> weak_thread_ctx;

        mt_mutex (HttpClient::mutex) bool valid;
        mt_mutex (HttpClient::mutex) bool connected;
        mt_mutex (HttpClient::mutex) List< Ref<HttpClientConnection> >::Element *conn_list_el;
        mt_mutex (mutex) PollGroup::PollableKey pollable_key;

        mt_sync (http_server)
        mt_begin
          Byte *preassembly_buf;
          Size  preassembly_buf_size;
          Size  preassembled_len;
        mt_end

        Embed<TcpConnection>            tcp_conn;
        Embed<DeferredConnectionSender> sender;
        Embed<ConnectionReceiver>       receiver;
        Embed<HttpServer>               http_server;

        mt_mutex (mutex) List< Ref<HttpClientRequest> > requests;

         HttpClientConnection (EmbedContainer *embed_container);
        ~HttpClientConnection ();
    };

    mt_const bool keepalive;
    mt_const Size preassembly_limit;

    mt_const Ref<ServerThreadContext> thread_ctx;
    mt_const Ref<PagePool> page_pool;

    mt_mutex (mutex) IpAddress cur_server_addr;
    mt_mutex (mutex) IpAddress next_server_addr;

    mt_mutex (mutex) StRef<String> host;

    mt_mutex (mutex) List< Ref<HttpClientConnection> > http_conns;

    mt_iface (TcpConnection::Frontend)
      static TcpConnection::Frontend const tcp_conn_frontend;

      static void connected (Exception *exc_,
                             void      *_http_conn);
    mt_iface_end

    mt_iface (Sender::Frontend)
      static Sender::Frontend const sender_frontend;

      static void senderStateChanged (SenderState  sender_state,
                                      void        *_http_conn);

      static void senderClosed (Exception *exc_,
                                void      *_http_conn);
    mt_iface_end

    mt_iface (HttpServer::Frontend)
      static HttpServer::Frontend const http_server_frontend;

      static void httpReply (HttpRequest * mt_nonnull reply,
                             bool        * mt_nonnull ret_block_input,
                             void        *_http_conn);

      static void httpReplyBody (HttpRequest * mt_nonnull reply,
                                 Memory       mem,
                                 bool         end_of_reply,
                                 Size        * mt_nonnull ret_accepted,
                                 bool        * mt_nonnull ret_block_input,
                                 void        *_http_conn);

      static void httpClosed (HttpRequest *reply,
                              Exception   *exc_,
                              void        *_http_conn);
    mt_iface_end

    mt_mutex (mutex) void destroyHttpClientConnection (HttpClientConnection * mt_nonnull http_conn,
                                                       HttpRequest          *reply);

    mt_mutex (mutex) Ref<HttpClientConnection> connect (bool *ret_connected);

    mt_mutex (mutex) Ref<HttpClientConnection> getConnection (bool *ret_connected,
                                                              bool  force_new_connection);

    mt_mutex (mutex) void sendRequest (HttpClientConnection * mt_nonnull http_conn,
                                       HttpClientRequest    * mt_nonnull http_req);

    Result queueRequest (HttpRequestType req_type,
                         ConstMemory     req_path,
                         ConstMemory     post_data,
                         CbDesc<HttpResponseHandler> const &response_cb,
                         bool            preassembly,
                         bool            parse_body_params,
                         bool            use_http_1_0);

  public:
    Result httpGet (ConstMemory req_path,
                    CbDesc<HttpResponseHandler> const &response_cb,
                    bool        preassembly       = false,
                    bool        parse_body_params = false,
                    bool        use_http_1_0      = false);

    Result httpPost (ConstMemory req_path,
                     ConstMemory post_data,
                     CbDesc<HttpResponseHandler> const &response_cb,
                     bool        preassembly       = false,
                     bool        parse_body_params = false,
                     bool        use_http_1_0      = false);

    void setServerAddr (IpAddress   server_addr,
                        ConstMemory host);

    mt_const void init (ServerThreadContext * mt_nonnull thread_ctx,
                        PagePool            * mt_nonnull page_pool,
                        IpAddress            server_addr,
                        ConstMemory          host,
                        bool                 keepalive,
                        Size                 preassembly_limit = 0);

     HttpClient (EmbedContainer *embed_container);
    ~HttpClient ();
};

}


#endif /* LIBMARY__HTTP_CLIENT__H__ */

