/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MESSAGE_SERVICE__H__
#define LIBMARY__MESSAGE_SERVICE__H__


#include <libmary/types.h>
#include <libmary/list.h>
#include <libmary/object.h>
#include <libmary/state_mutex.h>
#ifdef LIBMARY_XCODE_BUILD
  #include <libmary/tcp_server.linux.h>
  #include <libmary/tcp_connection.linux.h>
  #include <libmary/connection_receiver.generic.h>
#else
  #include <libmary/tcp_server.h>
  #include <libmary/tcp_connection.h>
  #include <libmary/connection_receiver.h>
#endif
#include <libmary/immediate_connection_sender.h>
#include <libmary/message_server.h>
#include <libmary/server_context.h>
#include <libmary/util_net.h>


namespace M {

class MessageService : public Object
{
private:
    StateMutex mutex;

    class Session;

public:
    typedef Session* SessionKey;
    struct SessionEvents;

    struct Frontend {
        void (*connected) (MessageService::SessionKey         session_key,
                           IpAddress                          client_addr,
                           Sender                            * mt_nonnull sender,
                           Cb<MessageService::SessionEvents> * mt_nonnull ret_cb,
                           void                              *cb_data);
    };

    struct SessionEvents {
        void (*readyToSend) (void *cb_data);

        void (*message) (Memory     msg,
                         IpAddress  client_addr,
                         Sender    *sender,
                         void      *cb_data);

        void (*closed) (void *cb_data);
    };

private:
    class Session : public Object
    {
    public:
        mt_mutex (mutex) bool destroyed;

        mt_const WeakRef<MessageService> weak_message_service;
        mt_const WeakRef<ServerThreadContext> weak_thread_ctx;

        mt_const IpAddress client_addr;
        mt_const Cb<SessionEvents> events_cb;

        Embed<TcpConnection>             tcp_conn;
        Embed<ImmediateConnectionSender> sender;
        Embed<AsyncStreamReceiver>       receiver;
        Embed<MessageServer>             msg_server;

        mt_mutex (mutex) PollGroup::PollableKey pollable_key;
        mt_mutex (mutex) List< Ref<Session> >::Element *session_list_el;

        Session (EmbedContainer *embed_container);
    };

    mt_const Ref<ServerContext> server_ctx;

    mt_const Cb<Frontend> frontend;
    mt_const Cb<SessionEvents> default_events_cb;
    mt_const Time max_session_lifetime_millisec;

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

  mt_iface (MessageServer::Frontend)
    static MessageServer::Frontend const msg_server_frontend;

    static void message (Memory  msg,
                         void   *_session);

    static void msgClosed (void *_session);
  mt_iface_end

    mt_unlocks (mutex) void destroySession (Session * mt_nonnull session);

    static void sessionLifetimeTimeout (void *_session);

    bool acceptOneConnection ();

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend const server_frontend;

    static void accepted (void *_self);
  mt_iface_end

public:
    mt_throws Result bind (IpAddress addr);

    mt_throws Result start ();

    mt_const mt_throws Result init (ServerContext               * mt_nonnull server_ctx,
                                    CbDesc<Frontend>      const &frontend,
                                    CbDesc<SessionEvents> const &default_events_cb,
                                    Time                         max_session_lifetime_millisec);

     MessageService (EmbedContainer *embed_container);
    ~MessageService ();
};

}


#endif /* LIBMARY__MESSAGE_SERVICE__H__ */

