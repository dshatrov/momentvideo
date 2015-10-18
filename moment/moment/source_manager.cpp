/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/fetch_agent.h>
#include <moment/slave_stream_source.h>

#include <moment/source_manager.h>


namespace Moment {

void
SourceManager::addPushProtocol (ConstMemory    const protocol_name,
                                PushProtocol * const mt_nonnull push_protocol)
{
    mutex.lock ();
    push_protocol_hash.add (protocol_name, push_protocol);
    mutex.unlock ();
}

void
SourceManager::addFetchProtocol (ConstMemory     const protocol_name,
                                 FetchProtocol * const mt_nonnull fetch_protocol)
{
    mutex.lock ();
    fetch_protocol_hash.add (protocol_name, fetch_protocol);
    mutex.unlock ();
}

Ref<PushProtocol>
SourceManager::getPushProtocolForUri (ConstMemory const uri)
{
    logD_ (_func, uri);

    ConstMemory protocol_name;
    {
        Count i = 0;
        for (Count const i_end = uri.len(); i < i_end; ++i) {
            if (uri.mem() [i] == ':')
                break;
        }
        protocol_name = uri.region (0, i);
    }

    Ref<PushProtocol> push_protocol;
    {
        mutex.lock ();
        PushProtocolHash::EntryKey const push_protocol_key = push_protocol_hash.lookup (protocol_name);
        if (push_protocol_key) {
            push_protocol = push_protocol_key.getData();
        }
        mutex.unlock ();
    }

    if (!push_protocol) {
        logE_ (_func, "Push protocol not found: ", protocol_name);
        return NULL;
    }

    return push_protocol;
}

Ref<FetchProtocol>
SourceManager::getFetchProtocolForUri (ConstMemory const uri)
{
    ConstMemory protocol_name;
    {
        Count i = 0;
        for (Count const i_end = uri.len(); i < i_end; ++i) {
            if (uri.mem() [i] == ':')
                break;
        }
        protocol_name = uri.region (0, i);
    }

    Ref<FetchProtocol> fetch_protocol;
    {
        mutex.lock ();
        FetchProtocolHash::EntryKey const fetch_protocol_key = fetch_protocol_hash.lookup (protocol_name);
        if (fetch_protocol_key) {
            fetch_protocol = fetch_protocol_key.getData();
        }
        mutex.unlock ();
    }

    if (!fetch_protocol) {
        logE_ (_func, "Fetch protocol not found: ", protocol_name);
        return NULL;
    }

    return fetch_protocol;
}

Ref<StreamSource>
SourceManager::createStreamSource (CbDesc<StreamSource::Frontend> const &frontend,
                                   Timers            * const timers,
                                   DeferredProcessor * const deferred_processor,
                                   PagePool          * const page_pool,
                                   AvStreamGroup     * const mt_nonnull stream_group,
                                   Time                const initial_seek,
                                   ChannelOptions    * const channel_opts,
                                   PlaybackItem      * const playback_item)
{
    if (playback_item->spec_kind == PlaybackItem::SpecKind::Slave) {
        Ref<StreamManager> const stream_manager = weak_stream_manager.getRef ();
        if (!stream_manager) {
            logD_ (_func, "stream_manager gone");
            return NULL;
        }

        Ref<SlaveStreamSource> const slave = grabNewObject <SlaveStreamSource> ();
        slave->init (stream_manager, playback_item->stream_spec->mem(), stream_group->getPrimaryStream(), frontend);
        return slave;
    } else
    if (playback_item->spec_kind == PlaybackItem::SpecKind::FetchUri) {
        Ref<FetchAgent> fetch_agent;

        Ref<FetchProtocol> const fetch_protocol = getFetchProtocolForUri (playback_item->stream_spec->mem());
        if (fetch_protocol) {
            Ref<ServerContext> const server_ctx = weak_server_ctx.getRef ();
            if (!server_ctx) {
                logD_ (_func, "server_ctx gone");
                return NULL;
            }

            fetch_agent = grabNewObject <FetchAgent> ();
            fetch_agent->init (server_ctx,
                               channel_opts,
                               playback_item,
                               fetch_protocol,
                               stream_group->getPrimaryStream(),
                               playback_item->stream_spec->mem(),
                               false /* do_reconnects */,
                               true  /* autostart */,
                               1000  /* reconnect_interval_millisec */ /* TODO Config parameter */,
                               CbDesc<FetchAgent::Frontend> (),
                               frontend);
        }

        return fetch_agent;
    }

    if (!stream_source_provider)
        return NULL;

    return stream_source_provider->createStreamSource (frontend,
                                                       timers,
                                                       deferred_processor,
                                                       page_pool,
                                                       stream_group,
                                                       initial_seek,
                                                       channel_opts,
                                                       playback_item);
}

mt_const void
SourceManager::init (StreamManager * const mt_nonnull stream_manager,
                     ServerContext * const mt_nonnull server_ctx)
{
    this->weak_stream_manager = stream_manager;
    this->weak_server_ctx     = server_ctx;
}

}

