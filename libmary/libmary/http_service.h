/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__HTTP_SERVICE__H__
#define LIBMARY__HTTP_SERVICE__H__


#include <libmary/types.h>
#include <libmary/namespace_container.h>
#include <libmary/timers.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/tcp_server.linux.h>
  #include <libmary/connection_receiver.generic.h>
#else
  #include <libmary/tcp_server.h>
  #include <libmary/connection_receiver.h>
#endif
#include <libmary/poll_group.h>
#include <libmary/immediate_connection_sender.h>
#include <libmary/http_server.h>
#include <libmary/address_saver.h>


namespace M {

class HttpService : public Object
{
  private:
    StateMutex mutex;

    class HttpConnection;

  public:
    typedef void UnblockInputCallback (void *cb_data);

    class HttpConnectionInfo
    {
        friend class HttpService;

        HttpConnection * const http_conn;
        bool           * const block_input_ptr;

        HttpConnectionInfo (HttpConnection * const http_conn,
                            bool           * const block_input_ptr)
            : http_conn       (http_conn),
              block_input_ptr (block_input_ptr)
        {}

      public:
        void blockInput () { *block_input_ptr = true; }

        CbDesc<UnblockInputCallback> getUnblockInputCb () const
        {
            return CbDesc<UnblockInputCallback> (unblockInput, http_conn, http_conn);
        }
    };

    struct HttpHandler
    {
      // TODO It's better to return 'bool' instead of 'Result', because it means
      //      'continue processing', not 'error occured'.

	// If the module has subscribed to request message body pre-assembly,
	// then @msg_body points to message body.
	Result (*httpRequest) (HttpRequest         * mt_nonnull req,
                               HttpConnectionInfo  * mt_nonnull conn_info,
                               IpAddress            local_addr,
			       Sender              * mt_nonnull sender,
			       Memory               msg_body,
			       void               ** mt_nonnull ret_msg_data,
			       void                *cb_data);

	// If mem.len() is 0, then we have not received the request in full due to
	// the connection being closed for some reason. For replies without
        // content-length header field, that is the normal end-of-reply notification.
        // In case of an error, such last call is made to give the module a chance
	// to release msg_data.
        //
        // Note that @req is NULL in case of some errors, e.g. http timeout.
	Result (*httpMessageBody) (HttpRequest        * mt_nonnull req,
                                   HttpConnectionInfo * mt_nonnull conn_info,
				   Sender             * mt_nonnull sender,
				   Memory              mem,
				   bool                end_of_request,
				   Size               * mt_nonnull ret_accepted,
				   void               *msg_data,
				   void               *cb_data);
    };

  private:
    class HandlerEntry
    {
      public:
	mt_const Cb<HttpHandler> cb;

	mt_const bool preassembly;
	mt_const Size preassembly_limit;
	mt_const bool parse_body_params;

	HandlerEntry ()
	{}
    };

    class HttpConnection : public Object,
			   public IntrusiveListElement<>
    {
      public:
	mt_mutex (mutex) bool valid;

	WeakRef<HttpService> weak_http_service;

	Embed<TcpConnection>             tcp_conn;
	Embed<ImmediateConnectionSender> sender;
	Embed<ConnectionReceiver>        receiver;
	Embed<HttpServer>                http_server;

        mt_const IpAddress local_addr;

	mt_mutex (mutex) PollGroup::PollableKey pollable_key;
	mt_mutex (mutex) Timers::TimerKey conn_keepalive_timer;

	// The following fields are synchroinzed by http_server.
	// They should only be accessed from HttpServer::Frontend callbacks.
	// {
	    HandlerEntry *cur_handler;
	    void *cur_msg_data;

            // Indicates that httpRequest() callback has already been called,
            // and httpMessageBody() should now be called.
            bool receiving_body;

	    Byte *preassembly_buf;
	    Size preassembly_buf_size;
	    Size preassembled_len;
	// }

	 HttpConnection (EmbedContainer *embed_container);
	~HttpConnection ();
    };

    mt_const Ref<PollGroup>         poll_group;
    mt_const Ref<Timers>            timers;
    mt_const Ref<DeferredProcessor> deferred_processor;
    mt_const Ref<PagePool>          page_pool;

    mt_mutex (mutex) Time keepalive_timeout_microsec;
    mt_mutex (mutex) bool no_keepalive_conns;

    Embed<TcpServer> tcp_server;
    AddressSaver addr_saver;

    typedef IntrusiveList<HttpConnection> ConnectionList;
    mt_mutex (mutex) ConnectionList conn_list;

    mt_mutex (mutex) NamespaceContainer<HandlerEntry> namespace_container;

    mt_mutex (mutex) void releaseHttpConnection (HttpConnection * mt_nonnull http_conn);
    mt_mutex (mutex) void destroyHttpConnection (HttpConnection * mt_nonnull http_conn);

    static void unblockInput (void *_http_conn);

    static void connKeepaliveTimerExpired (void *_http_conn);

    static void doCloseHttpConnection (HttpConnection *http_conn,
                                       HttpRequest    *req);

  mt_iface (HttpServer::Frontend)
    static HttpServer::Frontend const http_frontend;

    static void httpRequest (HttpRequest * mt_nonnull req,
                             bool        * mt_nonnull ret_block_input,
			     void        *_http_conn);

    static void httpMessageBody (HttpRequest * mt_nonnull req,
				 Memory       mem,
				 bool         end_of_request,
				 Size        * mt_nonnull ret_accepted,
                                 bool        * mt_nonnull ret_block_input,
				 void        *_http_conn);

    static void httpClosed (HttpRequest *req,
                            Exception   *exc_,
			    void        *_http_conn);
  mt_iface_end

    bool acceptOneConnection ();

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend const tcp_server_frontend;

    static void accepted (void *_self);
  mt_iface_end

public:
    void addHttpHandler (CbDesc<HttpHandler> const &cb,
			 ConstMemory path,
			 bool        preassembly       = false,
			 Size        preassembly_limit = 0,
			 bool        parse_body_params = false);

    mt_throws Result bind (IpAddress const &addr);

    mt_throws Result start ();

    void setConfigParams (Time keepalive_timeout_microsec,
                          bool no_keepalive_conns);

    AddressSaver* getAddressSaver () { return &addr_saver; }

    mt_throws Result init (PollGroup         * mt_nonnull poll_group,
			   Timers            * mt_nonnull timers,
                           DeferredProcessor * mt_nonnull deferred_processor,
			   PagePool          * mt_nonnull page_pool,
			   Time               keepalive_timeout_microsec,
                           bool               no_keeaplive_conns,
                           unsigned           max_saved_addrs = 4,
                           unsigned           max_saved_ports = 4);

     HttpService (EmbedContainer *embed_container);
    ~HttpService ();
};

}


#endif /* LIBMARY__HTTP_SERVICE__H__ */

