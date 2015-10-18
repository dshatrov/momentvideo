/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__RECORDER__H__
#define MOMENT__RECORDER__H__


#include <libmary/libmary.h>
#include <moment/playback.h>
#include <moment/av_recorder.h>
#include <moment/flv_muxer.h>
#include <moment/channel_manager.h>


namespace Moment {

using namespace M;

class Recorder : public Object
{
  private:
    StateMutex mutex;

    struct RecordingTicket : public Referenced
        { Recorder *recorder; };

    mt_const WeakRef<MomentServer>   weak_moment;
    mt_const WeakRef<StreamManager>  weak_stream_manager;
    mt_const WeakRef<ChannelManager> weak_channel_manager;

    mt_const StRef<String>  filename_prefix;
    mt_const ServerThreadContext *recorder_thread_ctx;

    mt_async Embed<Playback> playback;

    mt_async FlvMuxer flv_muxer;
    mt_async Embed<AvRecorder> recorder;

    mt_mutex (mutex) bool                 recording_now;
    mt_mutex (mutex) Ref<RecordingTicket> cur_recording_ticket;
    mt_mutex (mutex) StRef<String>        cur_channel_name;
    mt_mutex (mutex) WeakRef<Channel>     weak_cur_channel;
    mt_mutex (mutex) WeakRef<VideoStream> weak_cur_video_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey channel_sbn;

    mt_iface (Playback::Frontend)
      static Playback::Frontend playback_frontend;

      static void startPlaybackItem (Playlist::Item          *playlist_item,
                                     Time                     seek,
                                     Playback::AdvanceTicket *advance_ticket,
                                     void                    *_self);
    mt_iface_end

    mt_iface (Channel::ChannelEvents)
      static Channel::ChannelEvents channel_events;

      static void startChannelItem (MediaStream *new_stream,
                                    MediaStream *old_stream,
                                    void        *_recording_ticket);

      static void stopChannelItem  (MediaStream *new_stream,
                                    MediaStream *old_stream,
                                    void        *_recording_ticket);

      static void newVideoStream   (MediaStream *new_stream,
                                    MediaStream *old_stream,
                                    void        *_recording_ticket);
    mt_iface_end

    mt_mutex (mutex) void doStartItem ();
    mt_mutex (mutex) void doStopItem  ();

  public:
    void setSingleChannel (ConstMemory const channel_name)
    {
        playback->setSingleChannelRecorder (channel_name);
    }

    Result loadPlaylistFile (ConstMemory     const filename,
                             bool            const is_xml,
                             bool            const keep_cur_item,
                             PlaybackItem  * const mt_nonnull default_playback_item,
                             StRef<String> * const ret_err_msg)
    {
        return playback->loadPlaylistFile (filename, is_xml, keep_cur_item, default_playback_item, ret_err_msg);
    }

#if 0
// Unused
    Result loadPlaylistMem (ConstMemory     const mem,
                            bool            const is_xml,
                            bool            const keep_cur_item,
                            PlaybackItem  * const mt_nonnull default_playback_item,
                            StRef<String> * const ret_err_msg)
    {
        return playback->loadPlaylistMem (mem, is_xml, keep_cur_item, default_playback_item, ret_err_msg);
    }
#endif

    mt_const void init (MomentServer *moment,
                        PagePool     *page_pool,
                        ConstMemory   filename_prefix,
                        Uint64        min_playlist_duration_sec,
                        Uint64        pause_time_sec);

     Recorder (EmbedContainer *embed_container);
    ~Recorder ();
};

}


#endif /* MOMENT__RECORDER__H__ */

