/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/libmoment.h>


namespace MomentThumb {

using namespace Moment;

class ThumbService : public Object
{
  private:
    StateMutex mutex;

    class ChannelData : public Object,
                        public HashEntry<>
    {
      public:
        mt_const bool valid;
        mt_const WeakRef<ThumbService> weak_thumb_service;
        mt_const String channel_name;

        mt_mutex (mutex) WeakRef<MediaStream> weak_stream;

        VideoMessage prv_keyframe;

        PagePool::PageListHead cur_thumb_jpg;

        // TODO mt_mutex (mutex) WeakRef<MediaStream> cur_stream;
        // TODO data member to hold saved keyframe for the previous stream

        ChannelData (EmbedContainer * const embed_container)
            : Object (embed_container)
        {}
    };

    typedef Hash< ChannelData,
                  ConstMemory,
                  MemberExtractor< ChannelData,
                                   String,
                                   &ChannelData::channel_name,
                                   ConstMemory,
                                   AccessorExtractor< String,
                                                      ConstMemory,
                                                      &String::const_mem > >,
                  MemoryComparator<> >
            ChannelDataHash;

    mt_const Ref<Timers>   timers;
    mt_const Ref<PagePool> page_pool;

    Embed<FixedThreadPool> thumb_thread_pool;

    mt_const Time thumb_interval_millisec;

    mt_mutex (mutex) ChannelDataHash channel_hash;

    static void thumbTimerTick (void *_channel_data);

    mt_iface (ChannelManager::Events)
      static ChannelManager::Events const channel_manager_events;

      static void channelCreated (ChannelManager::ChannelInfo * mt_nonnull channel_info,
                                  void                        *_self);
    mt_iface_end

    static void doNewMediaStream (MediaStream *new_stream,
                                  MediaStream *old_stream,
                                  void        *_channel_data);

    mt_iface (Channel::ChannelEvents)
      static Channel::ChannelEvents const channel_events;

      static void channel_startItem (MediaStream *new_stream,
                                     MediaStream *old_stream,
                                     void        *_channel_data);

      static void channel_stopItem (MediaStream *new_stream,
                                    MediaStream *old_stream,
                                    void        *_channel_data);

      static void channel_newMediaStream (MediaStream *new_stream,
                                          MediaStream *old_stream,
                                          void        *_channel_data);

      static void channel_destroyed (void *_channel_data);
    mt_iface_end

    mt_iface (HttpService::HttpHandler)
      static HttpService::HttpHandler const http_handler;

      static Result httpRequest (HttpRequest  * mt_nonnull req,
                                 HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                                 IpAddress     local_addr,
                                 Sender       * mt_nonnull sender,
                                 Memory        msg_body,
                                 void        ** mt_nonnull ret_msg_data,
                                 void         *_self);
    mt_iface_end

  public:
    mt_const void init (MomentServer * mt_nonnull moment,
                        Time          thumb_interval_millisec);

     ThumbService (EmbedContainer *embed_container);
    ~ThumbService ();
};

}

