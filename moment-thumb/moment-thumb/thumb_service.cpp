/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-thumb/thumb_service.h>

// libav headers are in pure C
extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libswscale/swscale.h>
}


namespace MomentThumb {

void
ThumbService::thumbTimerTick (void * const _channel_data)
{
    ChannelData * const channel_data = static_cast <ChannelData*> (_channel_data);
    Ref<ThumbService> const self = channel_data->weak_thumb_service.getRef ();
    if (!self)
        return;

    logD_ (_self_func, "channel_data 0x", fmt_hex, (UintPtr) self.ptr());

  // TODO
}

ChannelManager::Events const ThumbService::channel_manager_events = {
    channelCreated
};

void
ThumbService::channelCreated (ChannelManager::ChannelInfo * const mt_nonnull channel_info,
                              void                        * const _self)
{
    ThumbService * const self = static_cast <ThumbService*> (_self);
    Channel * const channel = channel_info->channel;

    Ref<ChannelData> channel_data = grabNewObject <ChannelData> ();
    channel_data->valid = true;
    channel_data->weak_thumb_service = self;
    channel_data->channel_name = channel_info->channel_name;

    self->mutex.lock ();
    self->channel_hash.add (channel_data);

    self->timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (thumbTimerTick, channel_data, channel_data),
                                         /*time_microseconds=*/ 0,
                                         /*periodical=*/        false,
                                         /*auto_delete=*/       true,
                                         /*delete_after_tick=*/ true);

    assert (self->thumb_interval_millisec > 0);
    self->timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (thumbTimerTick, channel_data, channel_data),
                                         self->thumb_interval_millisec * 1000,
                                         /*periodical=*/        true,
                                         /*auto_delete=*/       true,
                                         /*delete_after_tick=*/ false);
    self->mutex.unlock ();

    channel->channelLock ();
    if (channel->isDestroyed_locked()) {
        channel->channelUnlock ();

        self->mutex.lock ();
        if (channel_data->valid) {
            channel_data->valid = false;
            self->channel_hash.remove (channel_data);
        }
        self->mutex.unlock ();
        return;
    }

    channel_data->weak_stream = channel->getMediaStream_locked();

    channel->getEventInformer()->subscribe_locked (
            CbDesc<Channel::ChannelEvents> (&channel_events, channel_data, channel_data));
    channel->channelUnlock ();

    channel_data.setNoUnref ((ChannelData*) NULL);
}

void
ThumbService::doNewMediaStream (MediaStream * const new_stream,
                                MediaStream * const old_stream,
                                void        * const _channel_data)
{
#if 0
    ChannelData * const channel_data = static_cast <ChannelData*> (_channel_data);
    Ref<ThumbService> const self = channel_data->weak_thumb_service.getRef ();
    if (!self)
        return;

    Object::Shadow * const new_stream_shadow = new_stream->getShadow();

    self->mutex.lock ();
    if (channel_data->weak_stream->getShadowPtr() == new_stream_shadow) {
        self->mutex.unlock ();
        return;
    }

    new_stream->lock ();
    bool keyframe_unchanged = true;

    FrameSaver * const new_frame_saver = new_stream->getFrameSaver();
    if (MediaDesc_Video * const new_media_desc__video = new_frame_saver->getMediaDesc_Video()) {
        if (!new_media_desc__video->equals (channel_data->media_desc__video))
            keyframe_unchanged = false;
    } else {
        if (channel_data->media_desc__video)
            keyframe_unchanged = false;
    }

    VideoMessage * const new_keyframe = new_frame_saver->getSavedKeyframe();
    if (keyframe_unchanged) {
        if (new_keyframe->page_list.first != channel_data->prv_keyframe.page_list.first)
            keyframe_unchanged = false;
    }

    if (keyframe_unchanged) {
        new_stream->unlock ();
        self->mutex.unlock ();
        return;
    }

    if (   new_keyframe->codec_id != VideoCodecId::AVC
        || !new_keyframe->page_list.first
        || !new_media_desc__video)
    {
        new_stream->unlock ();
        self->mutex.unlock ();
        return;
    }

    VideoMessage tmp_msg = channel_data->prv_keyframe;
    channel_data->prv_keyframe = *new_keyframe;
    channel_data->prv_keyframe.seize ();

    new_stream->unlock ();



    self->mutex.unlock ();

    tmp_msg.release ();

    return;
#endif

#if 0
#error lock stream
        && new_stream->getFrameSaver()->
    {
        self->mutex.unlock ();
        return;
    }

    self->mutex.unlock ();

    new_stream->
#endif
}

Channel::ChannelEvents const ThumbService::channel_events = {
    channel_startItem,
    channel_stopItem,
    channel_newMediaStream,
    channel_destroyed
};

void
ThumbService::channel_startItem (MediaStream * const new_stream,
                                 MediaStream * const old_stream,
                                 void        * const _channel_data)
{
    doNewMediaStream (new_stream, old_stream, _channel_data);
}

void
ThumbService::channel_stopItem (MediaStream * const new_stream,
                                MediaStream * const old_stream,
                                void        * const _channel_data)
{
    doNewMediaStream (new_stream, old_stream, _channel_data);
}

void
ThumbService::channel_newMediaStream (MediaStream * const new_stream,
                                      MediaStream * const old_stream,
                                      void        * const _channel_data)
{
    doNewMediaStream (new_stream, old_stream, _channel_data);
}

void
ThumbService::channel_destroyed (void * const _channel_data)
{
    ChannelData * const channel_data = static_cast <ChannelData*> (_channel_data);
    Ref<ThumbService> const self = channel_data->weak_thumb_service.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    if (!channel_data->valid) {
        self->mutex.unlock ();
        return;
    }
    channel_data->valid = false;

    self->channel_hash.remove (channel_data);
    self->mutex.unlock ();
}

HttpService::HttpHandler const ThumbService::http_handler =
{
    httpRequest,
    NULL /* httpMessageBody */
};

Result
ThumbService::httpRequest (HttpRequest  * const mt_nonnull req,
                           HttpService::HttpConnectionInfo * const mt_nonnull conn_info,
                           IpAddress      const /* local_addr */,
                           Sender       * const mt_nonnull sender,
                           Memory         const /* msg_body */,
                           void        ** const mt_nonnull /* ret_msg_data */,
                           void         * const _self)
{
    ThumbService * const self = static_cast <ThumbService*> (_self);

    logD_ (_self_func, req->getRequestLine());

  // TODO

    return Result::Success;
}

mt_const void
ThumbService::init (MomentServer * const mt_nonnull moment,
                    Time           const thumb_interval_millisec)
{
    this->timers    = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();
    this->page_pool = moment->getPagePool();
    this->thumb_interval_millisec = thumb_interval_millisec;

    if (thumb_interval_millisec > 0) {
        moment->getChannelManager()->getEventInformer()->subscribe (
                CbDesc<ChannelManager::Events> (&channel_manager_events, this, this));
    }

    moment->getHttpManager()->getHttpService()->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&http_handler, this, this),
            "thumb");
}

ThumbService::ThumbService (EmbedContainer * const embed_container)
    : Object                  (embed_container),
      thumb_thread_pool       (/*embed_container=*/ this),
      thumb_interval_millisec (0)
{
}

ThumbService::~ThumbService ()
{
    ChannelDataHash::NodeList tmp_channel_node_list;

    mutex.lock ();
    tmp_channel_node_list = *channel_hash.getNodeList ();
    mutex.unlock ();

    ChannelDataHash::NodeList::iterator iter (tmp_channel_node_list);
    while (!iter.done()) {
        ChannelData * const channel_data = iter.next ();
        channel_data->unref ();
    }
}

}

