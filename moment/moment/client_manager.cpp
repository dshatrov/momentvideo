/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_moment.h>
#include <moment/util_config.h>

#include <moment/client_manager.h>


namespace Moment {

void
ClientManager::sessionGuardDeleted (void * const _session)
{
    ClientSession * const session = static_cast <ClientSession*> (_session);
    Ref<ClientManager> const self = session->weak_client_manager.getRef ();
    if (!self)
        return;

    self->breakSession (session);
}


// ______________________________ startWatching() ______________________________

struct ClientManager::StartWatching_Data : public Referenced
{
    mt_const bool enable_vod;

    mt_const Ref<ClientSession> session;

    mt_once Ref<MediaSource> media_source;

    mt_const Cb<StartWatchingCallback> cb;
    mt_const WeakRef<Object>           weak_guard_obj;
};

static bool checkDomainAccess (ConstMemory     const referer_page_url,
                               MediaSource   * const mt_nonnull media_source)
{
    // TODO Check domains for VodSource as well.
    if (media_source->type == MediaSource::Type::MediaStream) {
        MediaStream * const stream = static_cast <MediaStream*> (media_source);

        if (Ref<DomainOptions> const domain_opts = stream->getDomainOptions()) {
            if (!isAllowedDomain (
                        extractDomainFromUrl (referer_page_url),
                                              &domain_opts->allowed_domains))
            {
                return false;
            }
        }
    }

    return true;
}

bool
ClientManager::startWatching (ClientRequestInfo                   * const mt_nonnull req_info,
                              bool                                  const enable_vod,
                              CbDesc<ClientSessionBackend> const  &session_backend,
                              Object                              * const guard_obj,
                              CbDesc<StartWatchingCallback> const &cb,
                              Ref<MediaSource>                    * const mt_nonnull ret_media_source,
                              Ref<ClientSession>                  * const mt_nonnull ret_session)
{
    logD_ (_func,
           "link \"", req_info->stream_link, "\", "
           "name \"", req_info->stream_name, "\"");

    *ret_media_source = NULL;
    *ret_session      = NULL;

    Ref<ClientSession> const session = grab (new (std::nothrow) ClientSession (/*embed_container=*/ NULL));
    {
        session->valid               = true;

        session->session_backend     = session_backend;
        session->weak_client_manager = this;
        session->hash_entry          = NULL;

        session->action_type         = ActionType::Watch;
        session->stream_name_as_requested = newString (req_info->stream_name);
        session->stream_link         = newString (req_info->stream_link);
        session->client_addr         = req_info->client_addr;
        session->referer_page_url    = newString (req_info->referer_page_url);
        session->module_name         = newString (req_info->module_name);
        session->protocol_name       = newString (req_info->protocol_name);
        session->rec_mode            = req_info->rec_mode;

        if (name_auth) {
            ConstMemory auth_id_mem = req_info->stream_name;
            if (!name_auth_full_path) {
                Byte const * const slash_pos = (Byte const *) memrchr (auth_id_mem.buf(), '/', auth_id_mem.len());
                if (slash_pos)
                    auth_id_mem = auth_id_mem.region (slash_pos - auth_id_mem.buf() + 1);
            }
            session->auth_id = newString (auth_id_mem);
        } else {
            session->auth_id     = newString (req_info->auth_id);
            session->stream_name = newString (req_info->stream_name);
        }
    }
    *ret_session = session;

    Ref<StartWatching_Data> const data = grabNew <StartWatching_Data> ();
    data->enable_vod     = enable_vod;
    data->session        = session;
    data->cb             = cb;
    data->weak_guard_obj = guard_obj;

    AuthManager::AuthAction auth_action = AuthManager::AuthAction::Watch;
    Ref<MediaSource> media_source;
    if (!name_auth) {
        media_source = media_manager->getMediaSource (session->stream_name, enable_vod);
        data->media_source = media_source;

        if (media_source) {
            if (!checkDomainAccess (session->referer_page_url, media_source)) {
                startWatching_completeBadReferer (this, data, false /* call_cb */);
                return true;
            }
        } else {
            if (   !enable_restreaming
                && !create_on_play)
            {
                startWatching_completeNotFound (this, data, false /* call_cb */);
                return true;
            }

            auth_action = AuthManager::AuthAction::WatchRestream;
        }
    }

    bool authorized = false;
    StRef<String> stream_name;
    StRef<String> restream_reply;
    if (!auth_manager->checkAuthorization (
                auth_action,
                session->stream_name,
                session->auth_id,
                session->client_addr,
                CbDesc<AuthManager::CheckAuthorizationCallback> (
                        startWatching_checkAuthorizationRet, data, this, data),
                &authorized,
                &stream_name,
                &restream_reply))
    {
        return false;
    }

    if (!authorized) {
        startWatching_completeDenied (this, data, false /* call_cb */);
        return true;
    }

    startWatching_completeOk (this,
                              data,
                              stream_name,
                              restream_reply,
                              false /* call_cb */,
                              ret_media_source);

    if (!*ret_media_source)
        *ret_media_source = media_source;

    *ret_session = session;
    return true;
}

void
ClientManager::startWatching_checkAuthorizationRet (bool          const authorized,
                                                    ConstMemory   const stream_name,
                                                    ConstMemory   const restream_reply,
                                                    void        * const _data)
{
    StartWatching_Data * const data = static_cast <StartWatching_Data*> (_data);
    Ref<ClientManager> const self = data->session->weak_client_manager.getRef ();
    if (!self) {
        startWatching_completeGone (self, data, true /* call_cb */);
        return;
    }

    if (!authorized) {
        startWatching_completeDenied (self, data, true /* call_cb */);
        return;
    }

    startWatching_completeOk (self,
                              data,
                              stream_name,
                              restream_reply,
                              true /* call_cb */,
                              NULL /* ret_media_source */);
}

void
ClientManager::startWatching_completeOk (ClientManager      * const self,
                                         StartWatching_Data * const mt_nonnull data,
                                         ConstMemory          const stream_name,
                                         ConstMemory          const restream_reply,
                                         bool                 const call_cb,
                                         Ref<MediaSource>   * const ret_media_source)
{
    if (!self) {
        startWatching_completeGone (self, data, call_cb);
        return;
    }

    if (self->name_auth) {
        assert (!data->session->stream_name);
        data->session->stream_name = newString (stream_name);

        Ref<MediaSource> const media_source =
                self->media_manager->getMediaSource (data->session->stream_name, data->enable_vod);
        data->media_source = media_source;

        if (media_source) {
            if (!checkDomainAccess (data->session->referer_page_url, media_source)) {
                startWatching_completeBadReferer (self, data, call_cb);
                return;
            }
        } else {
            if (   !self->enable_restreaming
                && !self->create_on_play)
            {
                startWatching_completeNotFound (self, data, call_cb);
                return;
            }
        }
    }

    if (!data->media_source) {
        if (!restream_reply.len()) {
            if (!self->create_on_play) {
                startWatching_completeNotFound (self, data, call_cb);
                return;
            }

//#warning TODO StreamManager::getOrCreateStream_Watch
            self->stream_manager->lock ();
            Ref<MediaStream> stream = self->stream_manager->getStream_locked (data->session->stream_name);
            if (stream) {
                ++stream->moment_data.use_count;
            } else {
                stream = grabNewObject <MediaStream> (self->deferred_processor, false /* publishing_in_progress */);

                MediaStream::MomentServerData * const stream_data = &stream->moment_data;
                stream_data->use_count = 1;
                stream_data->stream_info = grabNew <StreamManager::StreamInfo> ();

                StreamManager::StreamInfo * const stream_info = static_cast <StreamManager::StreamInfo*> (stream_data->stream_info.ptr());
                stream_info->waiting_for_streamer = true;

                stream_info->stream_key = self->stream_manager->addStream_locked (stream,
                                                                                  data->session->stream_name,
                                                                                  false /* fire_stream_added */);
                data->media_source = stream;
                if (ret_media_source)
                    *ret_media_source = stream;
            }
            self->stream_manager->unlock ();
        } else {
//#warning TODO startRestreaming()
//            data->media_source = self->startRestreaming (session->stream_name, restream_reply);
            if (!data->media_source) {
                startWatching_completeNotFound (self, data, call_cb);
                return;
            }

            if (ret_media_source)
                *ret_media_source = data->media_source;
        }
    } else {
//#warning TODO StreamManager::incStreamUseCount()
        if (data->media_source->type == MediaSource::Type::MediaStream) {
            MediaStream * const stream = static_cast <MediaStream*> (data->media_source.ptr());

            self->stream_manager->lock ();
            ++stream->moment_data.use_count;
            self->stream_manager->unlock ();
        }

        // There's no need to set *ret_media_source in this case.
    }

    // 'guard_obj' lifetime should extend to the end of this function.
    Ref<Object> guard_obj;
    if (data->weak_guard_obj.isValid()) {
        guard_obj = data->weak_guard_obj.getRef ();
        if (!guard_obj) {
            startWatching_completeGone (self, data, call_cb);
            return;
        }

        guard_obj->addDeletionCallback (CbDesc<Object::DeletionCallback> (sessionGuardDeleted, data->session, data->session));
    }

    logA_ ("moment OK ", data->session->client_addr, " watch ", data->session->stream_link);

    {
        ClientSessionHashEntry * const session_entry = new (std::nothrow) ClientSessionHashEntry;
        assert_hard (session_entry);
        session_entry->session = data->session;
        data->session->hash_entry = session_entry;

        self->mutex.lock ();

        data->session->client_id = makeString (self->client_id_counter);
        ++self->client_id_counter;

        self->session_hash.add (session_entry);
        ++self->num_sessions;

        self->mutex.unlock ();
    }

    if (call_cb) {
        if (!data->cb.call_ (data->media_source, data->session))
            self->breakSession (data->session);
    }
}

void
ClientManager::startWatching_completeNotFound (ClientManager      * const self,
                                               StartWatching_Data * const mt_nonnull data,
                                               bool                 const call_cb)
{
    startWatching_completeError (self, data, call_cb, "NOT_FOUND");
}

void
ClientManager::startWatching_completeDenied (ClientManager      * const self,
                                             StartWatching_Data * const mt_nonnull data,
                                             bool                 const call_cb)
{
    startWatching_completeError (self, data, call_cb, "DENIED");
}

void
ClientManager::startWatching_completeBadReferer (ClientManager      * const self,
                                                 StartWatching_Data * const mt_nonnull data,
                                                 bool                 const call_cb)
{
    startWatching_completeError (self, data, call_cb, "BAD_REFERER");
}

void
ClientManager::startWatching_completeGone (ClientManager      * const self,
                                           StartWatching_Data * const mt_nonnull data,
                                           bool                 const call_cb)
{
    startWatching_completeError (self, data, call_cb, "GONE");
}

void
ClientManager::startWatching_completeError (ClientManager      * const self,
                                            StartWatching_Data * const mt_nonnull data,
                                            bool                 const call_cb,
                                            ConstMemory          const error_str)
{
    logA_ ("moment ", error_str, " ", data->session->client_addr, " "
           "watch ", data->session->stream_link);

    if (call_cb)
        // TODO Should NULL be returned instead of data->session in failure cases for startWatching/startStreaming ?
        data->cb.call_ ((MediaSource*) NULL, data->session);

    if (self)
        self->breakSession (data->session);
}


// _____________________________ startStreaming() ______________________________

struct ClientManager::StartStreaming_Data : public Referenced
{
    Ref<MediaStream>           stream;
    Ref<ClientSession>         session;
    Cb<StartStreamingCallback> cb;
    WeakRef<Object>            weak_guard_obj;
};

bool
ClientManager::startStreaming (ClientRequestInfo const              * const mt_nonnull req_info,
                               MediaStream                          * const mt_nonnull stream,
                               CbDesc<ClientSessionBackend> const   &session_backend,
                               Object                               * const guard_obj,
                               CbDesc<StartStreamingCallback> const &cb,
                               Result                               * const mt_nonnull ret_res,
                               Ref<ClientSession>                   * const mt_nonnull ret_session)
{
    *ret_res = Result::Failure;
    *ret_session = NULL;

    Ref<ClientSession> const session = grab (new (std::nothrow) ClientSession (/*embed_container=*/ NULL));
    {
        session->valid               = true;

        session->session_backend     = session_backend;
        session->weak_client_manager = this;
        session->hash_entry          = NULL;

        session->action_type         = ActionType::Stream;
        session->stream_link         = newString (req_info->stream_link);
        session->client_addr         = req_info->client_addr;
        session->referer_page_url    = newString (req_info->referer_page_url);
        session->module_name         = newString (req_info->module_name);
        session->protocol_name       = newString (req_info->protocol_name);
        session->rec_mode            = req_info->rec_mode;

        if (name_auth) {
            session->auth_id     = newString (req_info->stream_name);
        } else {
            session->auth_id     = newString (req_info->auth_id);
            session->stream_name = newString (req_info->stream_name);
        }
    }

    Ref<StartStreaming_Data> const data = grabNew <StartStreaming_Data> ();
    data->stream = stream;
    data->session = session;
    data->cb = cb;
    data->weak_guard_obj = guard_obj;

    if (!enable_publish) {
        startStreaming_completeDenied (this, data, false /* call_cb */);
        return true;
    }

    bool authorized = false;
    StRef<String> stream_name;
    StRef<String> restream_reply;
    if (!auth_manager->checkAuthorization (
                    AuthManager::AuthAction::Stream,
                    session->stream_name,
                    session->auth_id,
                    session->client_addr,
                    CbDesc<AuthManager::CheckAuthorizationCallback> (
                            startStreaming_checkAuthorizationRet,
                            data,
                            NULL,
                            data),
                    &authorized,
                    &stream_name,
                    &restream_reply))
    {
        return false;
    }

    if (!authorized) {
        startStreaming_completeDenied (this, data, false /* call_cb */);
        return true;
    }

    startStreaming_completeOk (this, data, stream_name, false /* call_cb */);

    *ret_res = Result::Success;
    *ret_session = session;
    return true;
}

void
ClientManager::startStreaming_checkAuthorizationRet (bool          const authorized,
                                                     ConstMemory   const stream_name,
                                                     ConstMemory   const /* restream_reply */,
                                                     void        * const _data)
{
    StartStreaming_Data * const data = static_cast <StartStreaming_Data*> (_data);
    Ref<ClientManager> const self = data->session->weak_client_manager.getRef ();
    if (!self) {
        startStreaming_completeGone (self, data, true /* call_cb */);
        return;
    }

    if (!authorized) {
        startStreaming_completeDenied (self, data, true /* call_cb */);
        return;
    }

    startStreaming_completeOk (self, data, stream_name, true /* call_cb */);
}

void
ClientManager::startStreaming_completeOk (ClientManager       * const self,
                                          StartStreaming_Data * const mt_nonnull data,
                                          ConstMemory           const stream_name,
                                          bool                  const call_cb)
{
    if (!self) {
        startStreaming_completeGone (self, data, call_cb);
        return;
    }

    if (self->name_auth) {
        assert (!data->session->stream_name);
        data->session->stream_name = newString (stream_name);
    }

    {
        // TODO use_count - ?
        StreamManager::StreamKey const stream_key =
                self->stream_manager->addStream (data->stream,
                                                 data->session->stream_name,
                                                 true /* fire_stream_added */);

        self->mutex.lock ();
        bool remove = false;
        assert (!data->session->streaming_stream_key);
        if (data->session->valid) {
            data->session->streaming_stream_key = stream_key;
        } else {
            remove = true;
        }
        self->mutex.unlock ();

        if (remove) {
            self->stream_manager->removeStream (stream_key);
            startStreaming_completeGone (self, data, call_cb);
            return;
        }
    }

    if (data->weak_guard_obj.isValid()) {
        Ref<Object> const guard_obj = data->weak_guard_obj.getRef ();
        if (!guard_obj) {
            startStreaming_completeGone (self, data, call_cb);
            return;
        }

        guard_obj->addDeletionCallback (CbDesc<Object::DeletionCallback> (sessionGuardDeleted, data->session, data->session));
    }

    logA_ ("moment OK ", data->session->client_addr, " "
           "stream ", data->session->stream_name);

    {
        ClientSessionHashEntry * const session_entry = new (std::nothrow) ClientSessionHashEntry;
        assert_hard (session_entry);
        session_entry->session = data->session;
        data->session->hash_entry = session_entry;

        self->mutex.lock ();

        data->session->client_id = makeString (self->client_id_counter);
        ++self->client_id_counter;

        self->session_hash.add (session_entry);
        ++self->num_sessions;

        self->mutex.unlock ();
    }

    if (call_cb) {
        if (!data->cb.call_ (Result::Success, data->session))
            self->breakSession (data->session);
    }
}

void
ClientManager::startStreaming_completeDenied (ClientManager       * const self,
                                              StartStreaming_Data * const mt_nonnull data,
                                              bool                  const call_cb)
{
    startStreaming_completeError (self, data, call_cb, "DENIED");
}

void
ClientManager::startStreaming_completeGone (ClientManager       * const self,
                                            StartStreaming_Data * const mt_nonnull data,
                                            bool                  const call_cb)
{
    startStreaming_completeError (self, data, call_cb, "GONE");
}

void
ClientManager::startStreaming_completeError (ClientManager * const self,
                                             StartStreaming_Data * const mt_nonnull data,
                                             bool                  const call_cb,
                                             ConstMemory           const error_str)
{
    logA_ ("moment ", error_str, " ", data->session->client_addr, " "
           "stream ", data->session->stream_name);

    if (call_cb)
        data->cb.call_ (Result::Failure, data->session);

    if (self)
        self->breakSession (data->session);
}

// _____________________________________________________________________________


void
ClientManager::breakSession (ClientSession * const mt_nonnull session)
{
//#warning decrement use count?

    mutex.lock ();
    if (!session->valid) {
        mutex.unlock ();
        return;
    }
    session->valid = false;

    ClientSessionHashEntry *tmp_hash_entry = NULL;
    if (session->hash_entry) {
        tmp_hash_entry = session->hash_entry;
        session_hash.remove (session->hash_entry);
        session->hash_entry = NULL;
        --num_sessions;
    }

    StreamManager::StreamKey const stream_key = session->streaming_stream_key;
    session->streaming_stream_key = StreamManager::StreamKey();

    mutex.unlock ();

    if (stream_key)
        stream_manager->removeStream (stream_key);

//#warning this is mt_async action, but breakSession() is not marked as such.
    if (session->session_backend)
        session->session_backend.call (session->session_backend->breakSession);

    delete tmp_hash_entry;
}

mt_mutex (mutex) void
ClientManager::doDumpClient (OutputStream  * const mt_nonnull out,
                             ClientSession * const mt_nonnull session)
{
    out->print ("client ", session->client_id, " {\n"
                "    valid:            ",   session->valid, "\n"
                "    action_type:      ",   session->action_type, "\n"
                "    stream_name:      \"", session->stream_name, "\"\n"
                "    client_addr:      ",   session->client_addr, "\n"
                "    referer_page_url: \"", session->referer_page_url, "\"\n"
                "    module_name:      \"", session->module_name, "\"\n"
                "    protocol_name:    \"", session->protocol_name, "\"\n"
                "    auth_id:          \"", session->auth_id, "\"\n"
                "    rec_mode:         ",   session->rec_mode, "\n"
                "}\n");
}

mt_mutex (mutex) void
ClientManager::doDumpAllClients (OutputStream * const mt_nonnull out)
{
  // Note that log may be locked for the duration of the call.

    out->print ("num_sessions: ", num_sessions, "\n");

    ClientSessionHash::iterator iter (session_hash);
    while (!iter.done()) {
        ClientSession * const session = iter.next()->session;
        doDumpClient (out, session);
    }
}

void
ClientManager::dumpClient (OutputStream  * const mt_nonnull out,
                           ClientSession * const mt_nonnull session)
{
    mutex.lock ();
    doDumpClient (out, session);
    mutex.unlock ();
}

void
ClientManager::dumpClient (ClientSession * const mt_nonnull session)
{
  // We want to lock 'mutex' first, then logLock() to avoid locking order
  // inversion (it's always OK to log* with mutexes held, state mutexes
  // included).

    mutex.lock ();

    logLock ();
    doDumpClient (outs, session);
    logUnlock ();

    mutex.unlock ();
}

void
ClientManager::dumpAllClients (OutputStream * const mt_nonnull out)
{
    mutex.lock ();
    doDumpAllClients (out);
    mutex.unlock ();
}

void
ClientManager::dumpAllClients ()
{
  // We want to lock 'mutex' first, then logLock() to avoid locking order
  // inversion (it's always OK to log* with mutexes held, state mutexes
  // included).

    mutex.lock ();

    logLock ();
    doDumpAllClients (outs);
    logUnlock ();

    mutex.unlock ();
}

mt_const Result
ClientManager::init (DeferredProcessor * const mt_nonnull deferred_processor,
                     StreamManager * const mt_nonnull stream_manager,
                     MediaManager  * const mt_nonnull media_manager,
                     AuthManager   * const mt_nonnull auth_manager,
                     ConfigManager * const mt_nonnull config_manager)
{
    this->deferred_processor = deferred_processor;
    this->stream_manager = stream_manager;
    this->media_manager  = media_manager;
    this->auth_manager   = auth_manager;

    {
        Ref<MConfig::Config> const config = config_manager->getConfig ();

        if (!configGetBoolean (config, "moment/create_on_play", &create_on_play, create_on_play, _func))
            return Result::Failure;

        if (!configGetBoolean (config, "moment/name_auth", &name_auth, name_auth, _func))
            return Result::Failure;

        if (!configGetBoolean (config, "moment/name_auth_full_path", &name_auth_full_path, name_auth_full_path, _func))
            return Result::Failure;

        if (!configGetBoolean (config, "moment/enable_restreaming", &enable_restreaming, enable_restreaming, _func))
            return Result::Failure;

        // old spelling
        if (!configGetBoolean (config, "moment/publish_all", &enable_publish, enable_publish, _func))
            return Result::Failure;
        // new spelling
        if (!configGetBoolean (config, "moment/enable_publish", &enable_publish, enable_publish, _func))
            return Result::Failure;
    }

    return Result::Success;
}

ClientManager::ClientManager (EmbedContainer * const embed_container)
    : Object              (embed_container),
      event_informer      (this /* outer_object */, &mutex),
      create_on_play      (false),
      name_auth           (false),
      name_auth_full_path (false),
      enable_restreaming  (false),
      enable_publish      (true),
      num_sessions        (0),
      client_id_counter   (0)
{
}

ClientManager::~ClientManager ()
{
    for (;;) {
        // breakSession() invokes callbacks which may call breakSession()
        // for other sessions, hence we re-initialize the iterator
        // on every loop iteration (similar to getFirst()).
        ClientSessionHash::iterator iter (session_hash);
        if (iter.done())
            break;

        ClientSessionHashEntry * const session_entry = iter.next ();
        breakSession (session_entry->session);
    }
}

}

