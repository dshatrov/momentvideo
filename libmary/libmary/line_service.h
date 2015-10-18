/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LINE_SERVICE__H__
#define LIBMARY__LINE_SERVICE__H__


#include <libmary/list.h>
#include <libmary/server_context.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/tcp_server.linux.h>
  #include <libmary/tcp_connection.linux.h>
  #include <libmary/connection_receiver.generic.h>
#else
  #include <libmary/tcp_server.h>
  #include <libmary/tcp_connection.h>
  #include <libmary/connection_receiver.h>
#endif

#include <libmary/line_server.h>


namespace M {

class LineService : public Object
{
  private:
    StateMutex mutex;

  public:
    struct Frontend {
        void (*line) (ConstMemory  line,
                      void        *cb_data);
    };

  private:
    class LineConnection : public Object
    {
      public:
        typedef List< Ref<LineConnection> > LineConnectionList;

        mt_const WeakRef<LineService> weak_line_service;

        mt_mutex (LineService::mutex) LineConnectionList::Element *conn_list_el;

        mt_mutex (LineService::mutex) bool valid;
        mt_mutex (LineService::mutex) PollGroup::PollableKey pollable_key;

        Embed<TcpConnection>      tcp_conn;
        Embed<ConnectionReceiver> conn_receiver;
        Embed<LineServer>         line_server;

         LineConnection (EmbedContainer *embed_container);
        ~LineConnection ();
    };

    typedef LineConnection::LineConnectionList LineConnectionList;

    mt_const Size max_line_len;

    mt_const Ref<ServerThreadContext> thread_ctx;

    mt_const Cb<Frontend> frontend;

    Embed<TcpServer> tcp_server;
    mt_mutex (mutex) PollGroup::PollableKey server_pollable_key;

    mt_mutex (mutex) LineConnectionList conn_list;

    void releaseLineConnection (LineConnection * mt_nonnull line_conn);

    bool acceptOneConnection ();

    mt_iface (TcpServer::Frontend)
      static TcpServer::Frontend const tcp_server_frontend;

      static void accepted (void *_self);
    mt_iface_end

    mt_iface (LineServer::Frontend)
      static LineServer::Frontend const line_server_frontend;

      static void line (ConstMemory  line,
                        void        *_self);

      static void closed (void *_self);
    mt_iface_end

  public:
    mt_throws Result init ();

    mt_throws Result bind (IpAddress addr);

    mt_throws Result start ();

    mt_throws Result init (ServerContext          * mt_nonnull server_ctx,
                           CbDesc<Frontend> const &frontend,
                           Size                    max_line_len = 4096);

     LineService (EmbedContainer *embed_container);
    ~LineService ();
};

}


#endif /* LIBMARY__LINE_SERVICE__H__ */

