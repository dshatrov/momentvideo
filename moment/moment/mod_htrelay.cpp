/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/libmoment.h>


namespace MomentHtRelay {

using namespace Moment;

class MomentHtRelayModule : public Object
{
private:
    StateMutex mutex;

    class SessionGroup;

    class Session : public Referenced
    {
    public:
        mt_const Ref<SessionGroup> session_group;
        mt_const List< Session* >::Element *session_list_el;
    };

    class SessionGroup : public Referenced
    {
    public:
        mt_const  StringHash< Ref<SessionGroup> >::EntryKey session_group_hash_key;
        mt_mutex (mutex) List< Session* > session_list;
    };

    typedef StringHash< Ref<SessionGroup> > SessionGroupHash;
    mt_mutex (mutex) SessionGroupHash session_group_hash;

    mt_const Ref<PagePool> page_pool;

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const http_handler;

    static Result httpRequest (HttpRequest  * mt_nonnull req,
                               HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                               IpAddress     local_addr,
                               Sender       * mt_nonnull conn_sender,
                               Memory        msg_body,
                               void        ** mt_nonnull ret_msg_data,
                               void         *_self);

    static Result httpMessageBody (HttpRequest * mt_nonnull req,
                                   HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                                   Sender      * mt_nonnull conn_sender,
                                   Memory       mem,
                                   bool         end_of_request,
                                   Size        * mt_nonnull ret_accepted,
                                   void        *msg_data,
                                   void        *_self);
  mt_iface_end

public:
    mt_const Result init (MomentServer * mt_nonnull moment);

     MomentHtRelayModule (EmbedContainer *embed_container);
    ~MomentHtRelayModule ();
};

HttpService::HttpHandler const MomentHtRelayModule::http_handler = {
    httpRequest,
    httpMessageBody
};

Result
MomentHtRelayModule::httpRequest (HttpRequest  * const mt_nonnull req,
                                  HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
                                  IpAddress      const /* local_addr */,
                                  Sender       * const mt_nonnull conn_sender,
                                  Memory         const /* msg_body */,
                                  void        ** const mt_nonnull ret_msg_data,
                                  void         * const _self)
{
    MomentHtRelayModule * const self = static_cast <MomentHtRelayModule*> (_self);

    MOMENT_SERVER__HEADERS_DATE

    if (req->hasBody()) {
        Session * const session = new (std::nothrow) Session;
        assert (session);
        *ret_msg_data = session;

        logD_ (_func, "session 0x", fmt_hex, (UintPtr) session, " content-length ", req->getContentLength());

        self->mutex.lock ();
        if (SessionGroupHash::EntryKey const key = self->session_group_hash.lookup (req->getFullPath())) {
            Ref<SessionGroup> &session_group = *key.getDataPtr();

            session->session_group = session_group;
            session->session_list_el = session_group->session_list.append (session);
        } else {
            Ref<SessionGroup> const session_group = grab (new (std::nothrow) SessionGroup);
            session_group->session_group_hash_key = self->session_group_hash.add (req->getFullPath(), session_group);

            session->session_group = session_group;
            session->session_list_el = session_group->session_list.append (session);
        }
        self->mutex.unlock ();
    } else {
        logD_ (_func, "no body");
    }

    conn_sender->send (self->page_pool,
                       true /* do_flush */,
                       "HTTP/1.1 200 OK\r\n"
                       "Server: Moment/1.0\r\n",
                       "Date: ", ConstMemory (date_buf, date_len), "\r\n"
                       "Content-Type: text/plain\r\n"
//                       "Content-type: application/octet-stream\r\n"
                       "Content-Length: 0\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Connection: Keep-Alive\r\n"
                       "\r\n");

    if (equal (req->getMethod(), "HEAD")
        || !req->hasBody())
    {
        conn_sender->closeAfterFlush ();
    }

    logA_ ("htrelay 200 ", req->getClientAddress(), " ", req->getRequestLine());
    return Result::Success;
}

Result
MomentHtRelayModule::httpMessageBody (HttpRequest * const mt_nonnull req,
                                      HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
                                      Sender      * const mt_nonnull conn_sender,
                                      Memory        const mem,
                                      bool          const end_of_request,
                                      Size        * const mt_nonnull ret_accepted,
                                      void        * const _session,
                                      void        * const _self)
{
    MomentHtRelayModule * const self = static_cast <MomentHtRelayModule*> (_self);
    Session * const session = static_cast <Session*> (_session);

    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session, " len ", mem.len());

    if (end_of_request) {
        logD_ (_func, "session 0x", fmt_hex, (UintPtr) session, " end_of_request");

        self->mutex.lock ();

        assert (session->session_list_el);
        session->session_group->session_list.remove (session->session_list_el);
        session->session_list_el = NULL;

        if (session->session_group->session_list.isEmpty())
            self->session_group_hash.remove (session->session_group->session_group_hash_key);

        self->mutex.unlock ();

        session->unref ();

        if (!req->getKeepalive())
            conn_sender->closeAfterFlush ();
    }

    *ret_accepted = mem.len();
    return Result::Success;
}

mt_const Result
MomentHtRelayModule::init (MomentServer * const mt_nonnull moment)
{
    this->page_pool = moment->getPagePool();

    MConfig::Config * const config = moment->getConfigManager()->getConfig ();

    if (MConfig::Section * const section = config->getSection ("mod_htrelay/prefixes")) {
        MConfig::Section::iterator iter (*section);
        while (!iter.done()) {
            MConfig::SectionEntry * const entry = iter.next ();
            if (entry->getType() == MConfig::SectionEntry::Type_Option) {
                ConstMemory const prefix = entry->getName();
                logD_ (_func, "prefix: ", prefix);

                moment->getHttpManager()->getHttpService()->addHttpHandler (
                        CbDesc<HttpService::HttpHandler> (&http_handler, this, this),
                        prefix);
            }
        }
    }

    return Result::Success;
}

MomentHtRelayModule::MomentHtRelayModule (EmbedContainer * const embed_container)
    : Object (embed_container)
{
}

MomentHtRelayModule::~MomentHtRelayModule ()
{
  // TODO Unref sessions carefully.
}


static void serverDestroy (void * mt_nonnull _htrelay_module);

static MomentServer::Events const server_events (
    serverDestroy
);

static void serverDestroy (void * const _htrelay_module)
{
    MomentHtRelayModule * const htrelay_module = static_cast <MomentHtRelayModule*> (_htrelay_module);
    htrelay_module->unref ();
}

Result momentHtRelayInit ()
{
    MomentServer * const moment = MomentServer::getInstance();
    MConfig::Config * const config = moment->getConfigManager()->getConfig();

    {
        bool enable = false;
        if (!configGetBoolean (config, "mod_htrelay/enable", &enable, enable, _func))
            return Result::Failure;

        if (!enable) {
            logI_ (_func, "mod_htrelay is not enabled");
            return Result::Success;
        }
    }

    logI_ (_func, "initializing mod_htrelay");

    MomentHtRelayModule * const htrelay_module = new (std::nothrow) MomentHtRelayModule (NULL /* embed_container */);
    assert (htrelay_module);

    if (!htrelay_module->init (moment)) {
        logE_ (_func, "mod_htrelay module initialization failed");
        return Result::Failure;
    }

    moment->getEventInformer()->subscribe (
            CbDesc<MomentServer::Events> (&server_events, htrelay_module, NULL));

    return Result::Success;
}

void momentHtRelayUnload ()
{
    logI_ (_func, "unloading mod_htrelay");
}

}


#include <libmary/module_init.h>

extern "C" {

bool libMary_moduleInit ()
{
    return MomentHtRelay::momentHtRelayInit ();
}

void libMary_moduleUnload ()
{
    MomentHtRelay::momentHtRelayUnload ();
}

}

