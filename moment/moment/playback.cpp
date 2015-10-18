/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/inc.h>

#include <moment/playback.h>


using namespace M;

namespace Moment {

static LogGroup libMary_logGroup_playback ("moment.playback", LogLevel::I);

MOMENT__PLAYBACK

mt_unlocks_locks (mutex) void
Playback::advancePlayback ()
{
    logD (playback, _this_func_);

    if (advancing) {
        logD (playback, _this_func, "already advancing");
        return;
    }
    advancing = true;

    assert (got_next);
    bool first_iteration = true;
    while (got_next) {
        if (advance_ticket && advance_ticket->playback_timer) {
            timers->deleteTimer (advance_ticket->playback_timer);
            advance_ticket->playback_timer = NULL;
        }

        if (first_iteration) {
            first_iteration = false;

            // This is the only place where 'advance_ticket' is set, and it is
            // protected by 'advancing' flag. That's why we can safely
            // access 'access_ticket' from this function, in both locked and
            // unlocked contexts.
            advance_ticket = grab (new (std::nothrow) AdvanceTicket (this));
        }

        logD (playback, _this_func, "next_item: 0x", fmt_hex, (UintPtr) next_item.ptr());

        cur_item = next_item;
        if (next_item == (Playlist::Item*) NULL) {
            next_item = playlist.getNextItem (NULL /* prv_item */,
                                              getUnixtime(),
                                              0    /* time_offset */,
                                              &next_start_rel,
                                              &next_seek,
                                              &next_duration,
                                              &next_duration_full);
            got_next = true;

            if (next_item == (Playlist::Item*) NULL) {
                logD (playback, _this_func, "empty playlist");

                if (frontend) {
                    mt_unlocks_locks (mutex) frontend.call_mutex (
                            frontend->startItem, mutex, (Playlist::Item*) NULL /* item */, 0 /* seek */, advance_ticket);
                }

                goto _return;
            }

            {
                Time const unixtime = getUnixtime ();

                if (unixtime >= last_playlist_end_time
                    && unixtime - last_playlist_end_time < min_playlist_duration_sec)
                {
                    logW_ (_this_func, "playlist is shorter than ", min_playlist_duration_sec, " seconds; "
                           "pausing for ", pause_time_sec, " seconds");

                    if (frontend) {
                        mt_unlocks_locks (mutex) frontend.call_mutex (
                                frontend->startItem, mutex, (Playlist::Item*) NULL /* item */, 0 /* seek */, advance_ticket);
                    }

                    advance_ticket->playback_timer = timers->addTimer (
                            CbDesc<Timers::TimerCallback> (playbackTimerTick,
                                                           advance_ticket,
                                                           this,
                                                           advance_ticket),
                            pause_time_sec,
                            false /* periodical */,
                            true  /* auto_delete */);
                    goto _return;
                }

                last_playlist_end_time = unixtime;
            }

            // Jumping to the start of the playlist.
            continue;
        }

        logD (playback, _this_func, "next_start_rel: ", next_start_rel);
        if (next_start_rel > 0) {
            logD (playback, _this_func, "setting playback timer to ", next_start_rel, " (next_start_rel)");

            if (frontend) {
                mt_unlocks_locks (mutex) frontend.call_mutex (
                        frontend->startItem, mutex, (Playlist::Item*) NULL /* item */, 0 /* seek */, advance_ticket);
            }

            advance_ticket->playback_timer = timers->addTimer (
                    CbDesc<Timers::TimerCallback> (playbackTimerTick,
                                                   advance_ticket,
                                                   this,
                                                   advance_ticket),
                    next_start_rel,
                    false /* periodical */,
                    true  /* auto_delete */);

            next_start_rel = 0;
            got_next = true;
            goto _return;
        }

        if (!next_duration_full) {
            logD (playback, _this_func, "setting playback timer to ", next_duration, " (duration)");
            advance_ticket->playback_timer = timers->addTimer (
                    CbDesc<Timers::TimerCallback> (playbackTimerTick,
                                                   advance_ticket,
                                                   this,
                                                   advance_ticket),
                    next_duration,
                    false /* periodical */,
                    true  /* auto_delete */);
        }

        got_next = false;

        logD (playback, _this_func, "calling frontend->startItem()");
        if (frontend) {
            StRef<Playlist::Item> const tmp_next_item = next_item;
            Time const tmp_next_seek = next_seek;

            mt_unlocks_locks (mutex) frontend.call_mutex (
                    frontend->startItem, mutex, tmp_next_item, tmp_next_seek, advance_ticket);
        }
    } // while (got_next)

_return:
    advancing = false;
}

void
Playback::playbackTimerTick (void * const _advance_ticket)
{
    AdvanceTicket * const advance_ticket = static_cast <AdvanceTicket*> (_advance_ticket);
    Playback * const self = advance_ticket->playback;

    logD (playback, _func_);

    self->mutex.lock ();

    if (advance_ticket->playback_timer) {
        self->timers->deleteTimer (advance_ticket->playback_timer);
        advance_ticket->playback_timer = NULL;
    }

    if (self->advance_ticket != advance_ticket) {
        self->mutex.unlock ();
        return;
    }

    if (!self->got_next) {
        self->next_item = self->playlist.getNextItem (self->cur_item,
                                                      getUnixtime(),
                                                      0 /* time_offset */,
                                                      &self->next_start_rel,
                                                      &self->next_seek,
                                                      &self->next_duration,
                                                      &self->next_duration_full);
        self->got_next = true;
    }

    self->advancePlayback ();
    self->mutex.unlock ();
}

mt_mutex (mutex) void
Playback::doSetPosition (Playlist::Item * const item,
                         Time             const seek)
{
    logD (playback, _func_, ", seek: ", seek);

    Time duration = 0;
    if (!item->duration_full) {
        if (item->duration >= seek)
            duration = item->duration - seek;
        else
            duration = 0;
    }

    got_next = true;
    next_item = item;
    next_start_rel = 0;
    next_seek = seek;
    next_duration = duration;
    next_duration_full = item->duration_full || item->duration_default;

    advancePlayback ();
}

Result
Playback::doLoadPlaylist (PlaybackItem::SpecKind   const playlist_kind,
                          MConfig::Section       * const playlist_section,
                          ConstMemory              const src,
                          bool                     const keep_cur_item,
                          PlaybackItem           * const mt_nonnull default_playback_item,
                          StRef<String>          * const ret_err_msg,
                          bool                     const dir_re_read)
{
    logD (playback, _this_func_);

    mutex.lock ();

    playlist.clear ();

    cur_item = NULL;

    StRef<String> err_msg;
    Result res = Result::Failure;

    switch (playlist_kind.val()) {
        case PlaybackItem::SpecKind::PlaylistSection:
            res = playlist.parsePlaylist_ConfSection (playlist_section, default_playback_item);
            break;
        case PlaybackItem::SpecKind::PlaylistXml:
            res = playlist.parsePlaylistFile_XML (src, default_playback_item, &err_msg);
            break;
        case PlaybackItem::SpecKind::PlaylistConf:
            res = playlist.parsePlaylistFile_Conf (src, default_playback_item);
            break;
        case PlaybackItem::SpecKind::PlaylistDir:
            res = playlist.readDirectory (src, dir_re_read, default_playback_item);
            break;
        default:
            unreachable ();
    }

    if (!res) {
        mutex.unlock ();
        logE_ (_func, "could not load playlist: ", err_msg);

        if (ret_err_msg)
            *ret_err_msg = makeString ("playlist parsing error: ", err_msg->mem());

        return Result::Failure;
    }

    if (!keep_cur_item) {
        next_item = playlist.getNextItem (NULL /* prv_item */,
                                          getUnixtime(),
                                          0    /* time_offset */,
                                          &next_start_rel,
                                          &next_seek,
                                          &next_duration,
                                          &next_duration_full);
        got_next = true;
        advancePlayback ();
    }

    mutex.unlock ();

    return Result::Success;
}

void
Playback::advance (AdvanceTicket * const user_advance_ticket)
{
    logD (playback, _func_);

    mutex.lock ();
    if (user_advance_ticket != advance_ticket) {
        mutex.unlock ();
        return;
    }

    next_item = playlist.getNextItem (cur_item,
                                      getUnixtime(),
                                      0 /* time_offset */,
                                      &next_start_rel,
                                      &next_seek,
                                      &next_duration,
                                      &next_duration_full);
    got_next = true;
    advancePlayback ();
    mutex.unlock ();
}

void
Playback::restart ()
{
    mutex.lock ();
    playlist.clear ();
    next_item = playlist.getNextItem (NULL /* prv_item */,
                                      getUnixtime(),
                                      0    /* time_offset */,
                                      &next_start_rel,
                                      &next_seek,
                                      &next_duration,
                                      &next_duration_full);
    got_next = true;
    advancePlayback ();
    mutex.unlock ();
}

Result
Playback::setPosition_Id (ConstMemory const id,
                          Time        const seek)
{
    mutex.lock ();

    Playlist::Item * const item = playlist.getItemById (id);
    if (!item) {
        mutex.unlock ();
        logE_ (_func, "Item with id \"", id, "\" not found");
        return Result::Failure;
    }

    doSetPosition (item, seek);
    mutex.unlock ();

    return Result::Success;
}

Result
Playback::setPosition_Index (Count const idx,
                             Time  const seek)
{
    mutex.lock ();

    Playlist::Item * const item = playlist.getNthItem (idx);
    if (!item) {
        mutex.unlock ();
        logE_ (_func, "Item #", idx, " not found");
        return Result::Failure;
    }

    doSetPosition (item, seek);
    mutex.unlock ();

    return Result::Success;
}

void
Playback::setSingleItem (PlaybackItem * const item)
{
    logD (playback, _func_);

    mutex.lock ();
    playlist.clear ();
    playlist.setSingleItem (item);

    next_item = playlist.getNextItem (NULL /* prv_item */,
                                      getUnixtime(),
                                      0    /* time_offset */,
                                      &next_start_rel,
                                      &next_seek,
                                      &next_duration,
                                      &next_duration_full);
    got_next = true;
    advancePlayback ();
    mutex.unlock ();
}

void
Playback::setSingleChannelRecorder (ConstMemory const channel_name)
{
    logD (playback, _func_);

    mutex.lock ();
    playlist.clear ();
    playlist.setSingleChannelRecorder (channel_name);

    next_item = playlist.getNextItem (NULL /* prv_item */,
                                      getUnixtime(),
                                      0    /* time_offset */,
                                      &next_start_rel,
                                      &next_seek,
                                      &next_duration,
                                      &next_duration_full);
    got_next = true;
    advancePlayback ();
    mutex.unlock ();
}

Result
Playback::loadPlaylistFile (ConstMemory    const filename,
                            bool           const is_xml,
                            bool           const keep_cur_item,
                            PlaybackItem * const mt_nonnull default_playback_item,
                            StRef<String>  * const ret_err_msg)
{
    return doLoadPlaylist (is_xml ? PlaybackItem::SpecKind::PlaylistXml : PlaybackItem::SpecKind::PlaylistConf,
                           NULL  /* playlist_section */,
                           filename,
                           keep_cur_item,
                           default_playback_item,
                           ret_err_msg,
                           false /* dir_re_read */);
}

Result
Playback::loadPlaylistSection (MConfig::Section *playlist_section,
                               bool              keep_cur_item,
                               PlaybackItem     * mt_nonnull default_playback_item)
{
    return doLoadPlaylist (PlaybackItem::SpecKind::PlaylistSection,
                           playlist_section,
                           ConstMemory() /* src */,
                           keep_cur_item,
                           default_playback_item,
                           NULL  /* ret_err_msg */,
                           false /* dir_re_read */);
}

Result
Playback::loadPlaylistDirectory (ConstMemory    const dirname,
                                 bool           const re_read,
                                 bool           const keep_cur_item,
                                 PlaybackItem * const mt_nonnull default_playback_item)
{
    return doLoadPlaylist (PlaybackItem::SpecKind::PlaylistDir,
                           NULL /* playlist_section */,
                           dirname,
                           keep_cur_item,
                           default_playback_item,
                           NULL /* ret_err_msg */,
                           re_read);
}

mt_const void
Playback::init (CbDesc<Frontend> const &frontend,
                Timers * const timers,
                Uint64   const min_playlist_duration_sec,
                Uint64   const pause_time_sec)
{
    this->frontend = frontend;
    this->timers = timers;
    this->min_playlist_duration_sec = min_playlist_duration_sec;
    this->pause_time_sec = pause_time_sec;
}

Playback::Playback (EmbedContainer * const embed_container)
    : Object (embed_container),

      timers (NULL),

      min_playlist_duration_sec (10),
      pause_time_sec            (10),

      cur_item (NULL),

      got_next           (false),
      next_item          (NULL),
      next_start_rel     (0),
      next_seek          (0),
      next_duration      (0),
      next_duration_full (false),

      last_playlist_end_time (0),

      advancing (false)
{
}

Playback::~Playback ()
{
    mutex.lock ();
    mutex.unlock ();
}

}

