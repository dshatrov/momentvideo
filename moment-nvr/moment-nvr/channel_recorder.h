/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__CHANNEL_RECORDER__H__
#define MOMENT_NVR__CHANNEL_RECORDER__H__


#include <moment/libmoment.h>

#include <moment-nvr/media_recorder.h>
#include <moment-nvr/nvr_cleaner.h>


namespace MomentNvr {

class ChannelRecorder : public Object
{
  private:
    StateMutex mutex;

  public:
    enum ChannelResult
    {
        ChannelResult_Success,
        ChannelResult_Failure,
        ChannelResult_ChannelNotFound
    };

  private:
    class ChannelEntry : public Object
    {
      public:
        mt_const WeakRef<ChannelRecorder> weak_channel_recorder;

        mt_const Ref<ServerThreadContext> thread_ctx;
        mt_const ServerThreadContext *recorder_thread_ctx;

        mt_const Ref<Channel> channel;
        mt_const StRef<String> channel_name;

        mt_const Ref<MediaRecorder> media_recorder;
        mt_const Ref<NvrCleaner> nvr_cleaner;

        mt_const GenericStringHash::EntryKey hash_entry_key;

        mt_mutex (ChannelRecorder::mutex) bool valid;

        ChannelEntry (EmbedContainer * const embed_container)
            : Object (embed_container)
        {}
    };

    mt_const Ref<MomentServer>   moment;
    mt_const Ref<ChannelManager> channel_manager;
    mt_const StRef<Vfs>          vfs;
    mt_const Ref<NamingScheme>   naming_scheme;

    mt_const Time default_max_age_sec;
    mt_const Time clean_interval_sec;

    typedef StringHash< Ref<ChannelEntry> > ChannelHash;
    mt_mutex (mutex) ChannelHash channel_hash;

    mt_iface (Channel::ChannelEvents)
      static Channel::ChannelEvents const channel_events;

      static void channelStartItem      (MediaStream *new_stream,
                                         MediaStream *old_stream,
                                         void        *_channel_entry);

      static void channelStopItem       (MediaStream *new_stream,
                                         MediaStream *old_stream,
                                         void        *_channel_entry);

      static void channelNewVideoStream (MediaStream *new_stream,
                                         MediaStream *old_stream,
                                         void        *_channel_entry);

      static void channelDestroyed      (void *_channel_entry);
    mt_iface_end

    mt_iface (ChannelManager::Events)
      static ChannelManager::Events const channel_manager_events;

      static void channelCreated (ChannelManager::ChannelInfo * mt_nonnull channel_info,
                                  void *_self);
    mt_iface_end

    void doCreateChannel (ChannelManager::ChannelInfo * mt_nonnull channel_info);

    mt_mutex (mutex) void doDestroyChannel (ChannelEntry * mt_nonnull channel_entry);

  public:
    struct ChannelState
    {
        bool source_online;
        bool recording;
    };

    ChannelResult getChannelState (ConstMemory   channel_anme,
                                   ChannelState * mt_nonnull ret_state);

    ChannelResult setRecording (ConstMemory channel_name,
                                bool        set_on);

    mt_const void init (MomentServer * mt_nonnull moment,
                        Vfs          * mt_nonnull vfs,
                        NamingScheme * mt_nonnull naming_scheme,
                        Time          max_age_sec,
                        Time          clean_interval_sec);

     ChannelRecorder (EmbedContainer *embed_container);
    ~ChannelRecorder ();
};

}


#endif /* MOMENT_NVR__CHANNEL_RECORDER__H__ */

