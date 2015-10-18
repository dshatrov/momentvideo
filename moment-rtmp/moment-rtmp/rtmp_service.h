/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__RTMP_SERVICE__H__
#define LIBMOMENT__RTMP_SERVICE__H__


#include <libmary/libmary.h>

#include <moment-rtmp/rtmp_video_service.h>


//#define MOMENT__RTMP_SERVICE__USE_IMMEDIATE_SENDER


namespace Moment {

using namespace M;

class RtmpService : public RtmpVideoService,
		    public Object
{
private:
    StateMutex mutex;

public:
    class ClientSessionInfo
    {
    public:
        IpAddress client_addr;
        Time creation_unixtime;
        Time last_send_unixtime;
        Time last_recv_unixtime;
        StRef<String> last_play_stream;
        StRef<String> last_publish_stream;

        ClientSessionInfo ()
            : creation_unixtime  (0),
              last_send_unixtime (0),
              last_recv_unixtime (0)
        {}
    };

private:
    class SessionList_name;

    class ClientSession : public Object,
			  public IntrusiveListElement<SessionList_name>
    {
    public:
	bool valid;

	mt_const ServerThreadContext *thread_ctx;

	mt_const WeakRef<RtmpService> weak_rtmp_service;

	Embed<TcpConnection> tcp_conn;
     #ifdef MOMENT__RTMP_SERVICE__USE_IMMEDIATE_SENDER
	Embed<ImmediateConnectionSender> conn_sender;
     #else
	Embed<DeferredConnectionSender> conn_sender;
     #endif
	Embed<ConnectionReceiver> conn_receiver;
	Embed<RtmpConnection> rtmp_conn;

	mt_mutex (RtmpService::mutex) PollGroup::PollableKey pollable_key;

        mt_mutex (mutex) ClientSessionInfo session_info;

	ClientSession (EmbedContainer * const embed_container)
            : Object        (embed_container),
	      thread_ctx    (NULL),
	      tcp_conn      (this /* embed_container */),
	      conn_sender   (this /* embed_container */),
	      conn_receiver (this /* embed_container */),
	      rtmp_conn     (this /* embed_container */)
	{}

	~ClientSession ();
    };

    typedef IntrusiveList<ClientSession, SessionList_name> SessionList;

    mt_const ServerContext *server_ctx;
    mt_const PagePool *page_pool;
    mt_const Time send_delay_millisec;
    mt_const Time rtmp_ping_timeout_millisec;

    Embed<TcpServer> tcp_server;
    AddressSaver addr_saver;

    mt_mutex (mutex) SessionList session_list;

    AtomicInt num_session_objects;
    Count num_valid_sessions;

    mt_mutex (mutex) void destroySession (ClientSession *session);

  // ____ Accept watchdog ____

    mt_const Time accept_watchdog_timeout_sec;

    mt_mutex (mutex) Time last_accept_time;

    static void acceptWatchdogTick (void *_self);

  // _________________________

    bool acceptOneConnection ();

  mt_iface (RtmpConnection::Backend)
    static RtmpConnection::Backend const rtmp_conn_backend;

    static void closeRtmpConn (DisconnectReason  disconnect_reason,
                               void             *_session);
  mt_iface_end

  mt_iface (TcpServer::Frontend)
    static TcpServer::Frontend const tcp_server_frontend;

    static void accepted (void *_self);
  mt_iface_end

public:
    mt_throws Result bind (IpAddress addr);

    mt_throws Result start ();

    void rtmpServiceLock   () { mutex.lock (); }
    void rtmpServiceUnlock () { mutex.unlock (); }


  // _________________________ Current client sessions _________________________

private:
    mt_mutex (mutex) void updateClientSessionsInfo ();

public:
    class SessionInfoIterator
    {
        friend class RtmpService;

    private:
        SessionList::iterator iter;

        SessionInfoIterator (RtmpService const &rtmp_service) : iter (rtmp_service.session_list) {}

    public:
        SessionInfoIterator () {}

        bool operator == (SessionInfoIterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (SessionInfoIterator const &iter) const { return this->iter != iter.iter; }

        bool done () const { return iter.done(); }

        ClientSessionInfo* next ()
        {
            ClientSession * const session = iter.next ();
            return &session->session_info;
        }
    };

    struct SessionsInfo
    {
        Count num_session_objects;
        Count num_valid_sessions;
    };

    mt_mutex (mutex) SessionInfoIterator getClientSessionsInfo_locked (SessionsInfo *ret_info);

  // ___________________________________________________________________________


    AddressSaver* getAddressSaver () { return &addr_saver; }

    mt_const mt_throws Result init (ServerContext * mt_nonnull server_ctx,
                                    PagePool      * mt_nonnull page_pool,
                                    Time           send_delay_millisec,
                                    Time           rtmp_ping_timeout_millisec,
                                    Time           accept_watchdog_timeout_sec,
                                    unsigned       max_saved_addrs = 4,
                                    unsigned       max_saved_ports = 4);

     RtmpService (EmbedContainer *embed_container);
    ~RtmpService ();
};

}


#endif /* LIBMOMENT__RTMP_SERVICE__H__ */

