/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__EVENT_SERVICE__H__
#define MOMENT__EVENT_SERVICE__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class EventService : public Object
{
private:
    StateMutex mutex;

    class Session : public Object
    {
    public:
        mt_const WeakRef<EventService>        weak_event_service;
        mt_const WeakRef<ServerThreadContext> weak_thread_ctx;

      #ifndef LIBMARY_PLATFORM_WIN32
        Embed<NativeAsyncFile>          async_file;
      #endif
        Embed<TcpConnection>            tcp_conn;
        Embed<DeferredConnectionSender> sender;
        Embed<AsyncStreamReceiver>      receiver;
        Embed<LineServer>               line_server;

        mt_mutex (mutex) PollGroup::PollableKey pollable_key;
        mt_mutex (mutex) List< Ref<Session> >::Element *session_list_el;

        Session (EmbedContainer *embed_container);
    };

    mt_const Ref<ServerContext> server_ctx;
    mt_const Ref<PagePool> page_pool;

    Embed<TcpServer> tcp_server;

    mt_mutex (mutex) PollGroup::PollableKey server_pollable_key;
    mt_mutex (mutex) List< Ref<Session> > session_list;

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (SenderState  sender_state,
                                    void        *_session);

    static void senderClosed (Exception *exc_,
                              void      *_session);
  mt_iface_end

  mt_iface (LineServer::Frontend)
    static LineServer::Frontend const line_server_frontend;

    static void line (ConstMemory  line,
                      void        *_session);

    static void lineClosed (void *_session);
  mt_iface_end

    bool acceptOneConnection ();

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend const tcp_server_frontend;

    static void accepted (void *_self);
  mt_iface_end

    mt_mutex (mutex) void destroySession (Session * mt_nonnull session);

public:
  #ifndef LIBMARY_PLATFORM_WIN32
    mt_throws Result addOnewayPipeFd (int fd);
  #endif

    void sendEvent (ConstMemory line);

    mt_throws Result bind (IpAddress addr);

    mt_throws Result start ();

    mt_const void init (ServerContext * mt_nonnull server_ctx,
                        PagePool      * mt_nonnull page_pool);

    mt_const mt_throws Result initTcpServer ();

     EventService (EmbedContainer *embed_container);
    ~EventService ();
};

}


#endif /* MOMENT__EVENT_SERVICE__H__ */

