/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/inc.h>

#include <moment-nvr/channel_recorder.h>


namespace MomentNvr {

Channel::ChannelEvents const ChannelRecorder::channel_events = {
    channelStartItem,
    channelStopItem,
    channelNewVideoStream,
    channelDestroyed
};

void
ChannelRecorder::channelStartItem (MediaStream * const new_stream,
                                   MediaStream * const old_stream,
                                   void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    if (new_stream != old_stream) {
        channel_entry->media_recorder->setVideoStream (new_stream);
        channel_entry->media_recorder->startRecording ();
    }
}

void
ChannelRecorder::channelStopItem (MediaStream * const new_stream,
                                  MediaStream * const old_stream,
                                  void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    if (new_stream != old_stream) {
        channel_entry->media_recorder->setVideoStream (new_stream);
        channel_entry->media_recorder->startRecording ();
    }
}

void
ChannelRecorder::channelNewVideoStream (MediaStream * const new_stream,
                                        MediaStream * const /* old_stream */,
                                        void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    channel_entry->media_recorder->setVideoStream (new_stream);
    channel_entry->media_recorder->startRecording ();
}

void
ChannelRecorder::channelDestroyed (void * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    Ref<ChannelRecorder> const self = channel_entry->weak_channel_recorder.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    self->doDestroyChannel (channel_entry);
    self->mutex.unlock ();
}

ChannelManager::Events const ChannelRecorder::channel_manager_events = {
    channelCreated
};

void
ChannelRecorder::channelCreated (ChannelManager::ChannelInfo * const mt_nonnull channel_info,
                                 void * const _self)
{
    ChannelRecorder * const self = static_cast <ChannelRecorder*> (_self);

    logD_ (_func, "channel_name: ", channel_info->channel_name);
    self->doCreateChannel (channel_info);
}

// May be called with 'channel_manager' locked.
void
ChannelRecorder::doCreateChannel (ChannelManager::ChannelInfo * const mt_nonnull channel_info)
{
    logD_ (_func, "channel_name: ", channel_info->channel_name);

    Channel * const channel = channel_info->channel;

    bool nvr_record = true;
    {
        ConstMemory const opt_name = "nvr_record";
        if (!configSectionGetBoolean (channel_info->config_section, opt_name, &nvr_record, nvr_record,
                                      _func, "channel \"", channel_info->channel_name, "\": "))
        {
            return;
        }
        logI_ (_func, "channel \"", channel_info->channel_name, "\" ", opt_name, ": ", nvr_record);
    }
    if (!nvr_record)
        return;

    Uint64 prewrite_sec = 5;
    {
        ConstMemory const opt_name = "prewrite";
        if (!configSectionGetUint64 (channel_info->config_section, opt_name, &prewrite_sec, prewrite_sec,
                                      _func, "channel \"", channel_info->channel_name, "\": "))
        {
            return;
        }
        logI_ (_func, "channel \"", channel_info->channel_name, "\" ", opt_name, ": ", prewrite_sec);
    }

    Uint64 postwrite_sec = 5;
    {
        ConstMemory const opt_name = "postwrite";
        if (!configSectionGetUint64 (channel_info->config_section, opt_name, &postwrite_sec, postwrite_sec,
                                      _func, "channel \"", channel_info->channel_name, "\": "))
        {
            return;
        }
        logI_ (_func, "channel \"", channel_info->channel_name, "\" ", opt_name, ": ", postwrite_sec);
    }

    Uint64 prewrite_num_frames = prewrite_sec * 200;
    {
        ConstMemory const opt_name = "prewrite_frames";
        if (!configSectionGetUint64 (channel_info->config_section, opt_name, &prewrite_num_frames, prewrite_num_frames,
                                      _func, "channel \"", channel_info->channel_name, "\": "))
        {
            return;
        }
        logI_ (_func, "channel \"", channel_info->channel_name, "\" ", opt_name, ": ", prewrite_num_frames);
    }

    Uint64 postwrite_num_frames = postwrite_sec * 200;
    {
        ConstMemory const opt_name = "postwrite_frames";
        if (!configSectionGetUint64 (channel_info->config_section, opt_name, &postwrite_num_frames, postwrite_num_frames,
                                      _func, "channel \"", channel_info->channel_name, "\": "))
        {
            return;
        }
        logI_ (_func, "channel \"", channel_info->channel_name, "\" ", opt_name, ": ", postwrite_num_frames);
    }

    Uint64 max_age_minutes = default_max_age_sec / 60;
    {
        ConstMemory const opt_name = "max_age";
        if (!configSectionGetUint64 (channel_info->config_section, opt_name, &max_age_minutes, max_age_minutes,
                                     _func, "channel \"", channel_info->channel_name, "\": "))
        {
            return;
        }
        logI_ (_func, "channel \"", channel_info->channel_name, "\" ", opt_name, ": ", postwrite_num_frames);
    }
    Uint64 max_age_sec = max_age_minutes * 60;

    MOMENT_NVR__INIT

    Ref<ChannelEntry> const channel_entry = grabNewObject <ChannelEntry> ();
    channel_entry->valid = true;
    channel_entry->weak_channel_recorder = this;
    // TODO release thread_ctx
    channel_entry->thread_ctx = moment->getRecorderThreadPool()->grabThreadContext (ConstMemory() /* TODO filename */);
    if (channel_entry->thread_ctx) {
        channel_entry->recorder_thread_ctx = channel_entry->thread_ctx;
    } else {
        logE_ (_func, "Couldn't get recorder thread context: ", exc->toString());
        channel_entry->recorder_thread_ctx = NULL;
        channel_entry->thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
    }

    channel_entry->channel = channel;
    channel_entry->channel_name = st_grab (new (std::nothrow) String (channel_info->channel_name));

    channel_entry->media_recorder = grabNewObject <MediaRecorder> ();
    channel_entry->media_recorder->init (moment->getPagePool (),
                                         channel_entry->thread_ctx,
                                         vfs,
                                         naming_scheme,
                                         channel_info->channel_name,
                                         prewrite_sec * 1000000000,
                                         prewrite_num_frames,
                                         postwrite_sec * 1000000000,
                                         postwrite_num_frames);

    channel_entry->nvr_cleaner = grabNewObject <NvrCleaner> ();
    channel_entry->nvr_cleaner->init (moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                                      vfs,
                                      channel_info->channel_name,
                                      max_age_sec,
                                      clean_interval_sec);

    mutex.lock ();
//#warning TODO Deal with duplicate channel names.

    channel_entry->hash_entry_key = channel_hash.add (channel_info->channel_name, channel_entry);
    mutex.unlock ();

    {
        channel->channelLock ();
        if (channel->isDestroyed_locked()) {
            channel->channelUnlock ();

            mutex.lock ();
            doDestroyChannel (channel_entry);
            mutex.unlock ();
        } else {
            channel_entry->media_recorder->setVideoStream (channel->getMediaStream_locked());
            channel->getEventInformer()->subscribe_locked (
                    CbDesc<Channel::ChannelEvents> (&channel_events, channel_entry, channel_entry));
            channel->channelUnlock ();

            channel_entry->media_recorder->startRecording ();
        }
    }
}

mt_mutex (mutex) void
ChannelRecorder::doDestroyChannel (ChannelEntry * const mt_nonnull channel_entry)
{
    logD_ (_func, "channel: ", channel_entry->channel_name);

    if (!channel_entry->valid) {
        return;
    }
    channel_entry->valid = false;

    if (channel_entry->recorder_thread_ctx) {
        moment->getRecorderThreadPool()->releaseThreadContext (channel_entry->recorder_thread_ctx);
        channel_entry->recorder_thread_ctx = NULL;
    }

    channel_hash.remove (channel_entry->hash_entry_key);
    // 'channel_entry' is not valid anymore.
}

ChannelRecorder::ChannelResult
ChannelRecorder::getChannelState (ConstMemory    const channel_name,
                                  ChannelState * const mt_nonnull ret_state)
{
    mutex.lock ();
    ChannelHash::EntryKey const channel_entry_key = channel_hash.lookup (channel_name);
    if (!channel_entry_key) {
        mutex.unlock ();
        return ChannelResult_ChannelNotFound;
    }
    Ref<ChannelEntry> const &channel_entry = channel_entry_key.getData();

    ret_state->source_online = channel_entry->channel->isSourceOnline();
    ret_state->recording = channel_entry->media_recorder->isRecording();

    mutex.unlock ();
    return ChannelResult_Success;
}

ChannelRecorder::ChannelResult
ChannelRecorder::setRecording (ConstMemory const channel_name,
                               bool        const set_on)
{
    mutex.lock ();
    ChannelHash::EntryKey const channel_entry_key = channel_hash.lookup (channel_name);
    if (!channel_entry_key) {
        mutex.unlock ();
        return ChannelResult_ChannelNotFound;
    }
    Ref<ChannelEntry> const &channel_entry = channel_entry_key.getData();

    if (set_on)
        channel_entry->media_recorder->startRecording ();
    else
        channel_entry->media_recorder->stopRecording ();

    mutex.unlock ();
    return ChannelResult_Success;
}

mt_const void
ChannelRecorder::init (MomentServer * const mt_nonnull moment,
                       Vfs          * const mt_nonnull vfs,
                       NamingScheme * const mt_nonnull naming_scheme,
                       Time           const max_age_sec,
                       Time           const clean_interval_sec)
{
    logD_ (_func_);

    this->moment = moment;
    this->vfs = vfs;
    this->naming_scheme = naming_scheme;
    this->default_max_age_sec = max_age_sec;
    this->clean_interval_sec = clean_interval_sec;

    Ref<ChannelManager> const channel_manager = moment->getChannelManager();

    MOMENT_NVR__CHANNEL_RECORDER

    {
        channel_manager->channelManagerLock ();

        ChannelManager::ChannelInfo channel_info;
        ChannelManager::channel_iterator iter (channel_manager);
        while (!iter.done()) {
            logD_ (_func, "iteration");
            iter.next (&channel_info);
            logD_ (_func, "channel_name: ", channel_info.channel_name);
            doCreateChannel (&channel_info);
        }

        channel_manager->getEventInformer()->subscribe_locked (
                CbDesc<ChannelManager::Events> (&channel_manager_events, this, this));

        channel_manager->channelManagerUnlock ();
    }
}

ChannelRecorder::ChannelRecorder (EmbedContainer * const embed_container)
    : Object              (embed_container),
      default_max_age_sec (3600),
      clean_interval_sec  (5)
{
}

ChannelRecorder::~ChannelRecorder ()
{
    mutex.lock ();

    ChannelHash::iterator iter (channel_hash);
    while (!iter.done()) {
        Ref<ChannelEntry> * const channel_entry = iter.next ();
        doDestroyChannel (*channel_entry);
    }
    assert (channel_hash.isEmpty());

    mutex.unlock ();
}

}

