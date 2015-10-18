/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__CHANNEL_OPTIONS__H__
#define MOMENT__CHANNEL_OPTIONS__H__


#include <moment/playback_item.h>
#include <moment/domain_options.h>


namespace Moment {

class OverlayDesc : public Referenced
{
  public:
    StRef<String> overlay_channel_name;
    bool          pause_on_overlay;
};

class OverlayOptions : public Referenced
{
  public:
    List< Ref<OverlayDesc> > overlay_list;
};

class OutputDesc : public Referenced
{
  public:
    Ref<MConfig::Section> config_section;
};

class OutputOptions : public Referenced
{
  public:
    List< Ref<OutputDesc> > output_list;
};

class ChannelOptions : public Referenced
{
  public:
    mt_const
    mt_begin
      StRef<String> channel_name;
      StRef<String> channel_title;
      StRef<String> channel_desc;

      Ref<PlaybackItem> default_item;

      Ref<OverlayOptions> overlay_opts;
      Ref<OutputOptions>  output_opts;
      Ref<DomainOptions>  domain_opts;

      bool          hidden;

      bool          keep_video_stream;
      bool          continuous_playback;

      bool          recording;
      StRef<String> record_path;

      bool          connect_on_demand;
      Time          connect_on_demand_timeout;

      Time          no_video_timeout;
      bool          force_no_video_timeout;
      Time          min_playlist_duration_sec;
      Time          pause_time_sec;

      StRef<String> push_uri;
      StRef<String> push_username;
      StRef<String> push_password;
    mt_end

    // TODO Unused?
    void toConfigStringPages (PagePool               * mt_nonnull page_pool,
                              PagePool::PageListInfo * mt_nonnull page_list);

    template <class ...Args>
    void dump (LogLevel const loglevel,
               Args const &...args)
    {
        logLock ();
        log_locked_ (loglevel, args..., "channel_name: ",              channel_name);
        log_locked_ (loglevel, args..., "channel_title: ",             channel_title);
        log_locked_ (loglevel, args..., "channel_desc: ",              channel_desc);
        log_locked_ (loglevel, args..., "hidden: ",                    hidden);
        log_locked_ (loglevel, args..., "keep_video_stream: ",         keep_video_stream);
        log_locked_ (loglevel, args..., "continuous_playback: ",       continuous_playback);
        log_locked_ (loglevel, args..., "recording: ",                 recording);
        log_locked_ (loglevel, args..., "record_path: ",               record_path);
        log_locked_ (loglevel, args..., "connect_on_demand: ",         connect_on_demand);
        log_locked_ (loglevel, args..., "connect_on_demand_timeout: ", connect_on_demand_timeout);
        log_locked_ (loglevel, args..., "no_video_timeout: ",          no_video_timeout);
        log_locked_ (loglevel, args..., "force_no_video_timeout: ",    force_no_video_timeout);
        log_locked_ (loglevel, args..., "min_playlist_duration_sec: ", min_playlist_duration_sec);
        log_locked_ (loglevel, args..., "pause_time_sec: ",            pause_time_sec);
        log_locked_ (loglevel, args..., "push_uri: ",                  push_uri);
        log_locked_ (loglevel, args..., "push_username: ",             push_username);
        log_locked_ (loglevel, args..., "push_password: ",             push_password);
        logUnlock ();
    }

    ChannelOptions ()
        : channel_name  (newString ()),
          channel_title (newString ()),
          channel_desc  (newString ()),

          hidden (false),
           
          keep_video_stream (false),
          continuous_playback (true),

          recording (false),
          record_path (newString ()),

          connect_on_demand (false),
          connect_on_demand_timeout (60),

          no_video_timeout (30),
          force_no_video_timeout (false),
          min_playlist_duration_sec (10),
          pause_time_sec (10),

          push_uri      (newString ()),
          push_username (newString ()),
          push_password (newString ())
    {}
};

}


#endif /* MOMENT__CHANNEL_OPTIONS__H__ */

