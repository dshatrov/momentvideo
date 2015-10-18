/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__PLAYBACK_ITEM__H__
#define MOMENT__PLAYBACK_ITEM__H__


#include <mconfig/config.h>

#include <moment/moment_types.h>


namespace Moment {

class InputOptions : public Referenced
{
  public:
    Ref<MConfig::Section> config_section;
};

class PlaybackItem : public Referenced
{
  public:
    enum_beg (SpecKind)
        enum_values (
            None,
            Chain,
            Uri,
            FetchUri,
            Slave,
            PlaylistSection,
            PlaylistXml,
            PlaylistConf,
            PlaylistDir,
            Builtin
        )
    enum_end (SpecKind)

    mt_const
    mt_begin
      Ref<InputOptions> input_opts;

      StRef<String> stream_spec;
      SpecKind spec_kind;

      Ref<MConfig::Section> playlist_section;

      bool no_audio;
      bool no_video;

      bool force_transcode;
      bool force_transcode_audio;
      bool force_transcode_video;

      bool mp3_passthrough;

      bool aac_perfect_timestamp;

      bool sync_to_clock;
      bool sync_to_clock_specified;

      bool send_metadata;
      bool drop_sei_nal_units;
      bool dir_re_read;

      Uint64 avc_config_interval_millisec;
    mt_end

    bool noTranscodingFor_mp3 () const { return mp3_passthrough; }

    Uint64 getAvcConfigInterval_sec () const
    {
        if (avc_config_interval_millisec == 0)
            return 0;

        Uint64 const interval_sec = avc_config_interval_millisec / 1000;
        if (interval_sec == 0)
            return 1;

        return interval_sec;
    }

    // TODO Unused?
    void toConfigStringPages (PagePool               * mt_nonnull page_pool,
                              PagePool::PageListInfo * mt_nonnull page_list);

    template <class ...Args>
    void dump (LogLevel const loglevel,
               Args const &...args)
    {
        logLock ();
        log_locked_ (loglevel, args..., "stream_spec: ", stream_spec);
        log_locked_ (loglevel, args..., "spec_kind: ", spec_kind);
        log_locked_ (loglevel, args..., "no_audio: ", no_audio);
        log_locked_ (loglevel, args..., "no_video: ", no_video);
        log_locked_ (loglevel, args..., "force_transcode: ", force_transcode);
        log_locked_ (loglevel, args..., "force_transcode_audio: ", force_transcode_audio);
        log_locked_ (loglevel, args..., "force_transcode_video: ", force_transcode_video);
        log_locked_ (loglevel, args..., "mp3_passthrough: ", mp3_passthrough);
        log_locked_ (loglevel, args..., "aac_perfect_timestamp: ", aac_perfect_timestamp);
        log_locked_ (loglevel, args..., "sync_to_clock: ", sync_to_clock, " (", (sync_to_clock_specified ? "explicit" : "implicit"), ")");
        log_locked_ (loglevel, args..., "send_metadata: ", send_metadata);
        log_locked_ (loglevel, args..., "drop_sei_nal_units: ", drop_sei_nal_units);
        log_locked_ (loglevel, args..., "dir_re_read: ", dir_re_read);
        log_locked_ (loglevel, args..., "avc_config_interval: ", avc_config_interval_millisec);
        logUnlock ();
    }

    PlaybackItem ()
        : stream_spec (st_grab (new (std::nothrow) String)),
          spec_kind (SpecKind::None),

          no_audio (false),
          no_video (false),

          force_transcode (false),
          force_transcode_audio (false),
          force_transcode_video (false),

          mp3_passthrough (false),

          aac_perfect_timestamp (false),
          sync_to_clock (true),
          sync_to_clock_specified (false),

          send_metadata (false),
          drop_sei_nal_units (false),
          dir_re_read (true),
 
          avc_config_interval_millisec (1000)
    {}
};

}


#endif /* MOMENT__PLAYBACK_ITEM__H__ */

