/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MOMENT_SERVER__H__
#define MOMENT__MOMENT_SERVER__H__


#include <moment/stream_manager.h>
#include <moment/media_manager.h>
#include <moment/source_manager.h>
#include <moment/http_manager.h>
#include <moment/page_request_manager.h>
#include <moment/config_manager.h>
#include <moment/channel_manager.h>
#include <moment/transcoding_manager.h>
#include <moment/auth_manager.h>
#include <moment/client_manager.h>
#include <moment/local_storage.h>
#include <moment/event_service.h>


namespace Moment {

using namespace M;

// Only one MomentServer object may be initialized during program's lifetime.
// This limitation comes from the way loadable modules are implemented.
//
class MomentServer : public Object
{
  private:
    StateMutex mutex;


  // _________________________________ Events __________________________________

  public:
    struct Events
    {
        typedef void Destroy (void *cb_data);

        Destroy *destroy;

        constexpr Events (Destroy * const destroy)
            : destroy (destroy)
        {}
    };

  private:
    Informer_<Events> event_informer;

    static void informDestroy (Events * const events,
                               void   * const cb_data,
                               void   * const /* inform_data */)
    {
        if (events->destroy)
            events->destroy (cb_data);
    }

    void fireDestroy ()
        { event_informer.informAll (informDestroy, NULL /* inform_data */); }

  public:
    Informer_<Events>* getEventInformer () { return &event_informer; }

  // ___________________________________________________________________________


  private:
    mt_const Ref<ServerApp>        server_app;
    mt_const Ref<PagePool>         page_pool;
    mt_const Ref<ServerThreadPool> recorder_thread_pool;
    mt_const Ref<ServerThreadPool> reader_thread_pool;
    mt_const Ref<Storage>          storage;
    mt_const Ref<EventService>     event_service;

    Embed<StreamManager>      stream_manager;
    Embed<MediaManager>       media_manager;
    Embed<SourceManager>      source_manager;
    Embed<HttpManager>        http_manager;
    Embed<PageRequestManager> page_request_manager;
    Embed<ConfigManager>      config_manager;
    Embed<ChannelManager>     channel_manager;
    Embed<TranscodingManager> transcoding_manager;
    Embed<AuthManager>        auth_manager;
    Embed<ClientManager>      client_manager;

// DEPRECATED    static MomentServer *instance;

  public:
    ServerApp*          getServerApp          () { return server_app;           }
    PagePool*           getPagePool           () { return page_pool;            }
    ServerThreadPool*   getRecorderThreadPool () { return recorder_thread_pool; }
    ServerThreadPool*   getReaderThreadPool   () { return reader_thread_pool;   }
    Storage*            getStorage            () { return storage;              }
    EventService*       getEventService       () { return event_service;        }

    // TODO restreaming_manager
    StreamManager*      getStreamManager      () { return stream_manager;       }
    MediaManager*       getMediaManager       () { return media_manager;        }
    SourceManager*      getSourceManager      () { return source_manager;       }
    HttpManager*        getHttpManager        () { return http_manager;         }
    PageRequestManager* getPageRequestManager () { return page_request_manager; }
    ConfigManager*      getConfigManager      () { return config_manager;       }
    ChannelManager*     getChannelManager     () { return channel_manager;      }
    TranscodingManager* getTranscodingManager () { return transcoding_manager;  }
    AuthManager*        getAuthManager        () { return auth_manager;         }
    ClientManager*      getClientManager      () { return client_manager;       }

// DEPRECATED    static MomentServer* getInstance () { return instance; }

    mt_const Result init (ServerApp        * mt_nonnull server_app,
                          PagePool         * mt_nonnull page_pool,
                          ServerThreadPool * mt_nonnull recorder_thread_pool,
                          ServerThreadPool * mt_nonnull reader_thread_pool,
                          Storage          * mt_nonnull storage,
                          EventService     * event_service,
                          MConfig::Config  * mt_nonnull config);

     MomentServer (EmbedContainer *embed_container);
    ~MomentServer ();    
};

}


#endif /* MOMENT__MOMENT_SERVER__H__ */

