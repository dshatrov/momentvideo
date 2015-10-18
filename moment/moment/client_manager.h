/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__CLIENT_MANAGER__H__
#define MOMENT__CLIENT_MANAGER__H__


#include <moment/moment_types.h>
#include <moment/stream_manager.h>
#include <moment/media_manager.h>
#include <moment/auth_manager.h>
#include <moment/config_manager.h>


namespace Moment {

class ClientManager : public Object
{
  private:
    StateMutex mutex;

    struct ClientSessionHashEntry;

  public:
    enum_beg (ActionType)
        enum_values (Watch, Stream)
    enum_end (ActionType)

    struct ClientSessionBackend
    {
        void (*breakSession) (void *cb_data);
    };

    class ClientSession;

    struct ClientSessionEvents
    {
        void (*endSession) (ClientSession *session,
                            void          *cb_data);
    };

    class ClientSession : public Object
    {
        friend class ClientManager;

      private:
        StateMutex mutex;

      public:
        mt_const ActionType    action_type;
        mt_const StRef<String> stream_name_as_requested;
        mt_const StRef<String> stream_link;
        mt_const IpAddress     client_addr;
        mt_const StRef<String> referer_page_url;
        mt_const StRef<String> module_name;
        mt_const StRef<String> protocol_name;
        mt_const StRef<String> auth_id;
        mt_const RecordingMode rec_mode;

      private:
        mt_const Cb<ClientSessionBackend> session_backend;
        mt_const WeakRef<ClientManager>   weak_client_manager;

        mt_mutex (mutex) StRef<String> stream_name;

        // ClientSession is added to session_hash and gets its client_id after
        // successful authorization.
        mt_mutex (mutex) StRef<String> client_id;
        mt_mutex (mutex) ClientSessionHashEntry *hash_entry;

        mt_mutex (mutex) StreamManager::StreamKey streaming_stream_key;

        mt_mutex (mutex) bool valid;

        Informer_<ClientSessionEvents> event_informer;

        static void informEndSession (ClientSessionEvents * const events,
                                      void                * const cb_data,
                                      void                * const _self)
        {
            ClientSession * const self = static_cast <ClientSession*> (_self);
            events->endSession (self, cb_data);
        }

        void fireEndSession ()
        {
            event_informer.informAll (informEndSession, this);
        }

        ClientSession (EmbedContainer * const embed_container)
            : Object         (embed_container),
              event_informer (this /* outer_object */, &mutex)
        {}

      public:
        Informer_<ClientSessionEvents>* getEventIformer () { return &event_informer; }
    };

    class ClientMetaSession : public Referenced
    {
        friend class ClientManager;

        StateMutex client_meta_session_mutex;

        mt_const String auth_id;

        mt_mutex (client_meta_session_mutex) Time             timeout_millisec = 0;
        mt_mutex (client_meta_session_mutex) Timers::TimerKey timeout_timer;

        mt_mutex (client_meta_session_mutex) List< Ref<ClientSession> > client_session_list;
        mt_mutex (client_meta_session_mutex) Time no_sessions_time_millisec = 0;
    };

    class ClientMetaSessionHashEntry : public HashEntry<>
    {
        friend class ClientManager;

        Ref<ClientMetaSession> client_meta_session;
    };

    class ClientMetaSessionHash_Extractor
    {
      public:
        static ConstMemory getValue (ClientMetaSessionHashEntry const &entry)
                { return entry.client_meta_session->auth_id.const_mem(); }
    };

    typedef Hash< ClientMetaSessionHashEntry,
                  ConstMemory,
                  ClientMetaSessionHash_Extractor,
                  MemoryComparator<> >
            ClientMetaSessionHash;

    typedef void StartWatchingCallback (MediaSource   *media_source,
                                        ClientSession * mt_nonnull session,
                                        void          *cb_data);

    typedef void StartStreamingCallback (Result         result,
                                         ClientSession * mt_nonnull session,
                                         void          *cb_data);


  // _________________________________ Events __________________________________

    struct Events
    {
        void (*beginSession) (ClientSession *session,
                              void          *cb_data);
    };

    static void informBeginSession (Events * const events,
                                    void   * const cb_data,
                                    void   * const _session)
    {
        ClientSession * const session = static_cast <ClientSession*> (_session);
        events->beginSession (session, cb_data);
    }

    void fireBeginSession (ClientSession * const session)
    {
        event_informer.informAll (informBeginSession, session);
    }

  private:
    Informer_<Events> event_informer;

  public:
    Informer_<Events>* getEventInformer () { return &event_informer; }

  // ___________________________________________________________________________


  private:
    mt_const Ref<DeferredProcessor> deferred_processor;
    mt_const Ref<StreamManager> stream_manager;
    mt_const Ref<MediaManager>  media_manager;
    mt_const Ref<AuthManager>   auth_manager;

    mt_const bool create_on_play;
    mt_const bool name_auth;
    mt_const bool name_auth_full_path;
    mt_const bool enable_restreaming;
    mt_const bool enable_publish;

    struct ClientSessionHashEntry : public HashEntry<>
    {
        Ref<ClientSession> session;
    };

    typedef Hash< ClientSessionHashEntry,
                  ConstMemory,
                  MemberExtractor< ClientSessionHashEntry,
                                   Ref<ClientSession>,
                                   &ClientSessionHashEntry::session,
                                   ConstMemory,
                                   MemberExtractor< ClientSession,
                                                    StRef<String>,
                                                    &ClientSession::client_id,
                                                    ConstMemory,
                                                    AccessorExtractor< String,
                                                                       ConstMemory,
                                                                       &String::const_mem > > >,
                  MemoryComparator<> >
            ClientSessionHash;

    mt_mutex (mutex) ClientSessionHash session_hash;
    mt_mutex (mutex) Count num_sessions;

    // TODO Use IdMapper instead.
    mt_mutex (mutex) Uint64 client_id_counter;

    struct StartWatching_Data;

    static void startWatching_checkAuthorizationRet (bool         authorized,
                                                     ConstMemory  stream_name,
                                                     ConstMemory  restream_reply,
                                                     void        *_data);

    static void startWatching_completeOk         (ClientManager      *self,
                                                  StartWatching_Data * mt_nonnull data,
                                                  ConstMemory         stream_name,
                                                  ConstMemory         restream_reply,
                                                  bool                call_cb,
                                                  Ref<MediaSource>   *ret_media_source);

    static void startWatching_completeNotFound   (ClientManager      *self,
                                                  StartWatching_Data * mt_nonnull data,
                                                  bool                call_cb);

    static void startWatching_completeDenied     (ClientManager      *self,
                                                  StartWatching_Data * mt_nonnull data,
                                                  bool                call_cb);

    static void startWatching_completeBadReferer (ClientManager      *self,
                                                  StartWatching_Data * mt_nonnull data,
                                                  bool                call_cb);

    static void startWatching_completeGone       (ClientManager      *self,
                                                  StartWatching_Data * mt_nonnull data,
                                                  bool                call_cb);

    static void startWatching_completeError      (ClientManager      *self,
                                                  StartWatching_Data * mt_nonnull data,
                                                  bool                call_cb,
                                                  ConstMemory         error_str);

    struct StartStreaming_Data;

    static void startStreaming_checkAuthorizationRet (bool         authorized,
                                                      ConstMemory  stream_name,
                                                      ConstMemory  restream_reply,
                                                      void        *_data);

    static void startStreaming_completeOk     (ClientManager       *self,
                                               StartStreaming_Data * mt_nonnull data,
                                               ConstMemory          stream_name,
                                               bool                 call_cb);

    static void startStreaming_completeDenied (ClientManager       *self,
                                               StartStreaming_Data * mt_nonnull data,
                                               bool                 call_cb);

    static void startStreaming_completeGone   (ClientManager       *self,
                                               StartStreaming_Data * mt_nonnull data,
                                               bool                 call_cb);

    static void startStreaming_completeError  (ClientManager       *self,
                                               StartStreaming_Data * mt_nonnull data,
                                               bool                 call_cb,
                                               ConstMemory          error_str);

    static void sessionGuardDeleted (void *_session);

  public:
    struct ClientRequestInfo
    {
        // path-to-stream/ + stream-name + ?a=1&b=2&... args
        ConstMemory   stream_link;
        ConstMemory   stream_name;
        IpAddress     client_addr;
        ConstMemory   referer_page_url;
        // "mod_rtmp", "mod_rtsp", "mod_hls", "mod_nvr"
        ConstMemory   module_name;
        // "RTMP", "RTMPT", "HLS", "RTSP"
        ConstMemory   protocol_name;
        ConstMemory   auth_id;
        RecordingMode rec_mode;

        ClientRequestInfo ()
            : rec_mode (RecordingMode::NoRecording)
        {}
    };

    bool startWatching (ClientRequestInfo                   * mt_nonnull req_info,
                        bool                                 enable_vod,
                        CbDesc<ClientSessionBackend> const  &session_backend,
                        Object                              *guard_obj,
                        CbDesc<StartWatchingCallback> const &cb,
                        Ref<MediaSource>                    * mt_nonnull ret_source,
                        Ref<ClientSession>                  * mt_nonnull ret_session);

    bool startStreaming (ClientRequestInfo const              * mt_nonnull req_info,
                         MediaStream                          * mt_nonnull stream,
                         CbDesc<ClientSessionBackend> const   &session_backend,
                         Object                               *guard_obj,
                         CbDesc<StartStreamingCallback> const &cb,
                         Result                               * mt_nonnull ret_res,
                         Ref<ClientSession>                   * mt_nonnull ret_session);

    void breakSession (ClientSession * mt_nonnull session);


  // ______________________________ clients dump _______________________________

  private:
    mt_mutex (mutex) void doDumpClient (OutputStream  * mt_nonnull out,
                                        ClientSession * mt_nonnull session);

    mt_mutex (mutex) void doDumpAllClients (OutputStream * mt_nonnull out);

  public:
    void dumpClient (OutputStream  * mt_nonnull out,
                     ClientSession * mt_nonnull session);

    void dumpClient (ClientSession * mt_nonnull session);

    void dumpAllClients (OutputStream * mt_nonnull out);
    void dumpAllClients ();

  // ___________________________________________________________________________


    mt_const Result init (DeferredProcessor * mt_nonnull deferred_processor,
                          StreamManager     * mt_nonnull stream_manager,
                          MediaManager      * mt_nonnull media_manager,
                          AuthManager       * mt_nonnull auth_manager,
                          ConfigManager     * mt_nonnull config_manager);

     ClientManager (EmbedContainer *embed_container);
    ~ClientManager ();
};

}


#endif /* MOMENT__CLIENT_MANAGER__H__ */

