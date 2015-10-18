#if 0
// TODO Use numWatchersChanged instead.
void
MomentServer::decStreamUseCount (VideoStream * const stream)
{
    logD_ (_func, "stream 0x", fmt_hex, (UintPtr) stream);

    if (!stream)
        return;

    VideoStream::MomentServerData * const stream_data = &stream->moment_data;

    mutex.lock ();
    if (!stream_data->stream_info) {
        mutex.unlock ();
        return;
    }

    assert (stream_data->use_count > 0);
    --stream_data->use_count;

    if (stream_data->use_count == 0) {
        StreamInfo * const stream_info = static_cast <StreamInfo*> (stream_data->stream_info.ptr());
        if (RestreamInfo * const restream_info = stream_info->restream_info) {
            mt_unlocks (mutex) stopRestreaming (restream_info);
            return;
        } else
        if (stream_info->stream_key) {
            removeVideoStream_locked (stream_info->stream_key);
            stream_info->stream_key = VideoStreamKey();
        }
    }

    mutex.unlock ();
}
#endif

FetchAgent::Frontend const MomentServer::restream__fetch_agent_frontend = {
    restreamFetchAgentDisconnected
};

void
MomentServer::restreamFetchAgentDisconnected (void * const _restream_info)
{
    logD_ (_func_);

    RestreamInfo * const restream_info = static_cast <RestreamInfo*> (_restream_info);
    Ref<MomentServer> const self = restream_info->weak_moment.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    mt_unlocks (mutex) self->stopRestreaming (restream_info);
}

Ref<VideoStream>
MomentServer::startRestreaming (ConstMemory const stream_name,
                                ConstMemory const uri)
{
    logD_ (_func, "stream_name: ", stream_name, ", uri: ", uri);

    Ref<FetchProtocol> const fetch_proto = source_manager->getFetchProtocolForUri (uri);
    if (!fetch_proto) {
        logE_ (_func, "Could not get fetch protocol for uri: ", uri);
        return NULL;
    }

    mutex.lock ();

    Ref<Stream> stream = getMediaStream_locked (stream_name);
    if (stream) {
        logD_ (_func, "Stream \"", stream_name, "\" already exists");
        ++stream->moment_data.use_count;
        mutex.unlock ();
        return stream;
    }

    stream = grabNewObject <Stream> ();
    logD_ (_func, "Created stream 0x", fmt_hex, (UintPtr) stream.ptr(), ", "
           "stream_name: ", stream_name);

    VideoStream::MomentServerData * const stream_data = &stream->moment_data;
    stream_data->use_count = 1;
    stream_data->stream_info = grab (new (std::nothrow) StreamInfo);

    StreamInfo * const stream_info = static_cast <StreamInfo*> (stream_data->stream_info.ptr());
    stream_info->restream_info = grab (new (std::nothrow) RestreamInfo);
    stream_info->restream_info->weak_moment = this;
    stream_info->restream_info->unsafe_stream = stream;
    stream_info->restream_info->stream_key = addVideoStream_locked (stream, stream_name);

    // TODO Restreaming channel options
    Ref<ChannelOptions> const channel_opts = grab (new (std::nothrow) ChannelOptions);
    channel_opts->no_video_timeout = 0;

    Ref<PlaybackItem> const playback_item = grab (new (std::nothrow) PlaybackItem);
    channel_opts->default_item = playback_item;

    stream_info->restream_info->fetch_agent = grab (new (std::nothrow) FetchAgent);
    stream_info->restream_info->fetch_agent->init (this,
                                                   channel_opts,
                                                   playback_item,
                                                   fetch_proto,
                                                   stream,
                                                   uri,
                                                   false /* do_reconnects */,
                                                   true  /* autostart */,
                                                   0     /* reconnect_interval_millisec */,
                                                   CbDesc<FetchAgent::Frontend> (&restream__fetch_agent_frontend,
                                                                                 stream_info->restream_info,
                                                                                 stream_info->restream_info),
                                                   CbDesc<StreamSource::Frontend> ());
    mutex.unlock ();

    return stream;
}

mt_unlocks (mutex) void
MomentServer::stopRestreaming (RestreamInfo * const mt_nonnull restream_info)
{
    logD_ (_func_);

    Ref<VideoStream> stream;

    if (restream_info->stream_key) {
        stream = restream_info->unsafe_stream;
        removeVideoStream_locked (restream_info->stream_key);
        restream_info->stream_key = VideoStreamKey();
    }
    restream_info->fetch_agent = NULL;

    if (stream && stream->moment_data.stream_info) {
        if (StreamInfo * const stream_info =
                    static_cast <StreamInfo*> (stream->moment_data.stream_info.ptr()))
        {
            stream_info->restream_info = NULL;
        }
    }

    mutex.unlock ();

    if (stream)
        stream->close ();
}

