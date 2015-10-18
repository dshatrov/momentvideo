/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__CHANNEL_MANAGER__H__
#define MOMENT__CHANNEL_MANAGER__H__


#include <libmary/libmary.h>

#include <mconfig/mconfig.h>

#include <moment/moment_types.h>
#include <moment/config_manager.h>
#include <moment/http_manager.h>


namespace Moment {

class Channel;
class PushAgent;
class MomentServer;

class ChannelManager : public Object
{
  private:
    StateMutex mutex;

  public:
    struct ChannelInfo
    {
        Channel          *channel;
        ConstMemory       channel_name;
        MConfig::Section *config_section;
    };


  // _________________________________ Events __________________________________

  public:
    struct Events
    {
        void (*channelCreated) (ChannelInfo * mt_nonnull channel_info,
                                void        *cb_data);
    };

  private:
    Informer_<Events> event_informer;

    static void informChannelCreated (Events * const events,
                                      void   * const cb_data,
                                      void   * const _channel_info)
    {
        if (events->channelCreated) {
            ChannelInfo * const channel_info = static_cast <ChannelInfo*> (_channel_info);
            events->channelCreated (channel_info, cb_data);
        }
    }

    void fireChannelCreated (ChannelInfo * const mt_nonnull channel_info)
        { event_informer.informAll (informChannelCreated, channel_info); }

  public:
    Informer_<Events>* getEventInformer () { return &event_informer; }

  // ___________________________________________________________________________


  private:
    struct ConfigItem : public StReferenced
    {
        Ref<Channel> channel;
        StRef<String> channel_name;
        StRef<String> channel_title;

        Ref<MConfig::Config> config;
        MConfig::Section *config_section;

        Ref<PushAgent> push_agent;
    };

    mt_const WeakRef<MomentServer> weak_moment;
    mt_const Ref<ConfigManager>    config_manager;

    mt_const Ref<PagePool> page_pool;

    mt_const StRef<String> confd_dirname;
    mt_const bool          serve_playlist_json;
    mt_const StRef<String> playlist_json_protocol;

    typedef StringHash< StRef<ConfigItem> > ItemHash;
    mt_mutex (mutex) ItemHash item_hash;

    mt_mutex (mutex) Ref<OutputDesc> default_output_desc;
    mt_mutex (mutex) Ref<ChannelOptions> default_channel_opts;

    mt_iface (HttpManager::HttpRequestHandler)
      static HttpManager::HttpRequestHandler admin_http_handler;

      static HttpManager::HttpRequestResult adminHttpRequest (HttpRequest * mt_nonnull req,
                                                              IpAddress    local_addr,
                                                              Sender      * mt_nonnull conn_sender,
                                                              Memory       msg_body,
                                                              void        *cb_data);
    mt_iface_end

    mt_iface (HttpManager::HttpRequestHandler)
      static HttpManager::HttpRequestHandler server_http_handler;

      static HttpManager::HttpRequestResult serverHttpRequest (HttpRequest * mt_nonnull req,
                                                               IpAddress    local_addr,
                                                               Sender      * mt_nonnull conn_sender,
                                                               Memory       msg_body,
                                                               void        *cb_data);
    mt_iface_end


  // _____________________ Channel creation notification ______________________

    struct ChannelCreationMessage : public IntrusiveListElement<>
    {
        Ref<Channel>  channel;
        StRef<String> channel_name;

        Ref<MConfig::Config>  config;
        MConfig::Section     *config_section;
    };

    typedef IntrusiveList< ChannelCreationMessage > ChannelCreationMessageList;

    mt_mutex (mutex) ChannelCreationMessageList channel_creation_messages;

    DeferredProcessor::Task channel_created_task;
    DeferredProcessor::Registration deferred_reg;

    void notifyChannelCreated (ChannelInfo * mt_nonnull channel_info);

    static bool channelCreatedTask (void *_self);

  // ___________________________________________________________________________


    Result loadStreamsSection ();
    Result loadConfdDir ();

    Result parseConfigItem (MomentServer     * mt_nonnull moment,
                            ConstMemory       item_name,
                            MConfig::Config  * mt_nonnull config,
                            MConfig::Section * mt_nonnull section);

  public:
    void channelManagerLock   () { mutex.lock (); }
    void channelManagerUnlock () { mutex.unlock (); }

    Result loadConfigFull ();

    Result loadConfigItem (ConstMemory item_name,
                           ConstMemory item_path);

    Result updatePlaylist (ConstMemory    channel_name,
                           bool           keep_cur_item,
                           StRef<String> * mt_nonnull ret_err_msg);

    Result setPosition (ConstMemory channel_name,
                        ConstMemory item_name,
                        bool        item_name_is_id,
                        ConstMemory seek_str);


  // ____________________________ channel_iterator _____________________________

  private:
    static void fillChannelInfo (ConfigItem  * mt_nonnull item,
                                 ChannelInfo * mt_nonnull ret_info);

  public:
    class channel_iterator
    {
      private:
        ItemHash::iterator iter;

      public:
        channel_iterator (ChannelManager * const mt_nonnull cm) : iter (cm->item_hash) {}
        channel_iterator () {};

        bool operator == (channel_iterator const &iter) const { return this->iter == iter.iter; }
        bool operator != (channel_iterator const &iter) const { return this->iter != iter.iter; }

        bool done () const { return iter.done(); }

        void next (ChannelInfo * const mt_nonnull ret_info)
        {
            ConfigItem * const item  = iter.next ()->ptr();
            fillChannelInfo (item, ret_info);
        }
    };

  // ___________________________________________________________________________


    // TODO Deprecated
    StRef<String> getConfdDirname () { return makeString (confd_dirname); }

    mt_mutex (mutex) Result getChannelInfo_locked (ConstMemory  channel_name,
                                                   ChannelInfo * mt_nonnull ret_info);


  // _____________________________ config changes ______________________________

    // TODO File op synchronization, separate thread for vfs/file operations.

    Result renameChannel (ConstMemory old_name,
                          ConstMemory new_name);

    Result saveChannel (ConstMemory channel_name,
                        ConstMemory config_mem);

    Result removeChannel (ConstMemory channel_name);

  // ___________________________________________________________________________


    mt_const void init (MomentServer * mt_nonnull moment,
                        PagePool     * mt_nonnull page_pool);

     ChannelManager (EmbedContainer *embed_container);
    ~ChannelManager ();
};

}


#include <moment/channel.h>
#include <moment/push_agent.h>
#include <moment/moment_server.h>


#endif /* MOMENT__CHANNEL_MANAGER__H__ */

