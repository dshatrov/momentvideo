/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__PLAYBACK__H__
#define MOMENT__PLAYBACK__H__


#include <libmary/libmary.h>

#include <moment/playlist.h>


namespace Moment {

using namespace M;

class Playback : public Object
{
  private:
    StateMutex mutex;

  public:
    class AdvanceTicket : public Referenced
    {
        friend class Playback;

      private:
        Playback * const playback;
        mt_mutex (mutex) Timers::TimerKey playback_timer;
        AdvanceTicket (Playback * const playback) : playback (playback) {}
    };

    struct Frontend
    {
        void (*startItem) (Playlist::Item *item,
                           Time            seek,
                           AdvanceTicket  *advance_ticket,
                           void           *cb_data);
    };

  private:
    mt_const Cb<Frontend> frontend;

    mt_const Timers *timers;

    mt_const Uint64 min_playlist_duration_sec;
    mt_const Uint64 pause_time_sec;

    mt_mutex (mutex)
    mt_begin
      Playlist playlist;

      StRef<Playlist::Item> cur_item;

      bool got_next;
      StRef<Playlist::Item> next_item;
      Time next_start_rel;
      Time next_seek;
      Time next_duration;
      bool next_duration_full;

      Time last_playlist_end_time;

      Ref<AdvanceTicket> advance_ticket;

      bool advancing;
    mt_end

    mt_unlocks_locks (mutex) void advancePlayback ();

    static void playbackTimerTick (void *_advance_ticket);

    mt_mutex (mutex) void doSetPosition (Playlist::Item *item,
                                         Time            seek);

    Result doLoadPlaylist (PlaybackItem::SpecKind  playlist_kind,
                           MConfig::Section       *playlist_section,
                           ConstMemory             src,
                           bool                    keep_cur_item,
                           PlaybackItem          * mt_nonnull default_playback_item,
                           StRef<String>         *ret_err_msg,
                           bool                   dir_re_read);

  public:
    void advance (AdvanceTicket *user_advance_ticket);

    void restart ();

    Result setPosition_Id (ConstMemory id,
                           Time        seek);

    Result setPosition_Index (Count idx,
                              Time  seek);

    void setSingleItem (PlaybackItem *item);

    void setSingleChannelRecorder (ConstMemory channel_name);

    Result loadPlaylistFile (ConstMemory   filename,
                             bool          is_xml,
                             bool          keep_cur_item,
                             PlaybackItem * mt_nonnull default_playback_item,
                             StRef<String>  *ret_err_msg);

    Result loadPlaylistSection (MConfig::Section *playlist_section,
                                bool              keep_cur_item,
                                PlaybackItem     * mt_nonnull default_playback_item);

    Result loadPlaylistDirectory (ConstMemory   dirname,
                                  bool          re_read,
                                  bool          keep_cur_item,
                                  PlaybackItem * mt_nonnull default_playback_item);

    mt_const void init (CbDesc<Frontend> const &frontend,
                        Timers *timers,
                        Uint64  min_playlist_duration_sec,
                        Uint64  pause_time_sec);

     Playback (EmbedContainer *embed_container);
    ~Playback ();
};

}


#endif /* MOMENT__PLAYBACK__H__ */

