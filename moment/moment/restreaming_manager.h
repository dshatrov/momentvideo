#ifndef MOMENT__RESTREAMING_MANAGER__H__
#define MOMENT__RESTREAMING_MANAGER__H__


#include <moment/fetch_agent.h>


namespace Moment {

class RestreamingManager : public Object
{
#if 0
    class StreamInfo : public Referenced
    {
    public:
        mt_mutex (mutex) Ref<RestreamInfo> restream_info;
        mt_mutex (mutex) bool              waiting_for_streamer;

        // For restreamed streams, RestreamInfo::stream_key is used instead.
        mt_mutex (mutex) VideoStreamKey    stream_key;

        StreamInfo ()
            : waiting_for_streamer (false)
        {}
    };
#endif

private:
    class RestreamInfo : public Object
    {
      public:
        mt_const WeakRef<MomentServer> weak_moment;

//        mt_mutex (mutex) VideoStreamKey stream_key;
        // Valid when 'stream_key' is non-null, which means that
        // video_stream_hash holds a reference to the stream.
        mt_mutex (mutex) VideoStream *unsafe_stream;

        mt_mutex (mutex) Ref<FetchAgent> fetch_agent;
    };

  mt_iface (FetchAgent::Frontend)
    static FetchAgent::Frontend const restream__fetch_agent_frontend;

    static void restreamFetchAgentDisconnected (void *_restream_info);
  mt_iface_end

public:
    Ref<VideoStream> startRestreaming (ConstMemory stream_name,
                                       ConstMemory uri);

private:
    mt_unlocks (mutex) void stopRestreaming (RestreamInfo * mt_nonnull restream_info);
};

}


#endif /* MOMENT__RESTREAMING_MANAGER__H__ */

