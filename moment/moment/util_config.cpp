/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/libmoment.h>

#include <moment/util_config.h>


namespace Moment {

Result
parseOverlayConfig (MConfig::Section * const mt_nonnull channel_section,
                    ChannelOptions   * const mt_nonnull opts)
{
    MConfig::Section * const overlay_section = channel_section->getSection ("overlay");
    if (!overlay_section)
        return Result::Success;

    if (!opts->overlay_opts)
        opts->overlay_opts = grab (new (std::nothrow) OverlayOptions);

    MConfig::Section::iterator iter (*overlay_section);
    while (!iter.done()) {
        bool pause_on_overlay = true;

        MConfig::SectionEntry * const sect_entry = iter.next ();
        if (sect_entry->getType() == MConfig::SectionEntry::Type_Section) {
            MConfig::Section * const section = static_cast <MConfig::Section*> (sect_entry);

            if (!configSectionGetBoolean (section, "pause", &pause_on_overlay, pause_on_overlay, _func))
                return Result::Failure;
        }

        Ref<OverlayDesc> const overlay_desc = grab (new (std::nothrow) OverlayDesc);
        overlay_desc->overlay_channel_name = st_grab (new (std::nothrow) String (sect_entry->getName()));
        overlay_desc->pause_on_overlay = pause_on_overlay;

        opts->overlay_opts->overlay_list.append (overlay_desc);
    }

    return Result::Success;
}

static ConstMemory
itemToStreamName (ConstMemory const item_name)
{
    ConstMemory stream_name = item_name;
    for (Size i = stream_name.len(); i > 0; --i) {
        if (stream_name.mem() [i - 1] == '.') {
            stream_name = stream_name.region (0, i - 1);
            break;
        }
    }

    return stream_name;
}

static Result
parseChannelOptionsConfig (MConfig::Section * const mt_nonnull section,
                           ConstMemory        const config_item_name,
                           ChannelOptions   * const mt_nonnull default_opts,
                           ChannelOptions   * const mt_nonnull opts)
{
    char const opt_name__name[]                      = "name";
    char const opt_name__title[]                     = "title";
    char const opt_name__desc[]                      = "desc";
    char const opt_name__hidden[]                    = "hidden";
    char const opt_name__keep_video_stream[]         = "keep_video_stream";
    char const opt_name__continuous_playback[]       = "continuous_playback";
    char const opt_name__record_path[]               = "record_path";
    char const opt_name__connect_on_demand[]         = "connect_on_demand";
    char const opt_name__connect_on_demand_timeout[] = "connect_on_demand_timeout";
    char const opt_name__no_video_timeout[]          = "no_video_timeout";
    char const opt_name__force_no_video_timeout[]    = "force_no_video_timeout";
    char const opt_name__min_playlist_duration[]     = "min_playlist_duration";
    char const opt_name__pause_time[]                = "pause_time";
    char const opt_name__push_uri[]                  = "push_uri";
    char const opt_name__push_username[]             = "push_username";
    char const opt_name__push_password[]             = "push_password";

    ConstMemory channel_name = itemToStreamName (config_item_name);
    {
        MConfig::Section::attribute_iterator attr_iter (*section);
        MConfig::Attribute *name_attr = NULL;
        if (!attr_iter.done()) {
            MConfig::Attribute * const attr = attr_iter.next ();
            if (!attr->hasValue()) {
                channel_name = attr->getName();
                name_attr = attr;
            }
        }

        if (MConfig::Attribute * const attr = section->getAttribute (opt_name__name)) {
            if (attr != name_attr)
                channel_name = attr->getValue ();
        }
    }
    logD_ (_func, opt_name__name, ": ", channel_name);

    ConstMemory channel_title = String::isNullString (default_opts->channel_title) ?
                                        channel_name : default_opts->channel_title->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__title)) {
        if (opt->getValue())
            channel_title = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__title, ": ", channel_title);

    ConstMemory channel_desc = default_opts->channel_desc->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__desc)) {
        if (opt->getValue())
            channel_desc = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__desc, ": ", channel_desc);

    bool hidden = default_opts->hidden;
    if (!configSectionGetBoolean (section,
                                  opt_name__hidden,
                                  &hidden,
                                  hidden,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__hidden, ": ", hidden);

    bool keep_video_stream = default_opts->keep_video_stream;
    if (!configSectionGetBoolean (section,
                                  opt_name__keep_video_stream,
                                  &keep_video_stream,
                                  keep_video_stream,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__keep_video_stream, ": ", keep_video_stream);

    bool continuous_playback = default_opts->continuous_playback;
    if (!configSectionGetBoolean (section,
                                  opt_name__continuous_playback,
                                  &continuous_playback,
                                  continuous_playback,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__continuous_playback, ": ", continuous_playback);

    ConstMemory record_path = default_opts->record_path->mem();
    bool got_record_path = false;
    if (MConfig::Option * const opt = section->getOption (opt_name__record_path)) {
        if (opt->getValue()) {
            record_path = opt->getValue()->mem();
            got_record_path = true;
        }
    }
    logD_ (_func, opt_name__record_path, ": ", record_path);

    bool connect_on_demand = default_opts->connect_on_demand;
    if (!configSectionGetBoolean (section,
                                  opt_name__connect_on_demand,
                                  &connect_on_demand,
                                  connect_on_demand,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__connect_on_demand, ": ", connect_on_demand);

    Uint64 connect_on_demand_timeout = default_opts->connect_on_demand_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__connect_on_demand_timeout,
                                 &connect_on_demand_timeout,
                                 connect_on_demand_timeout,
                                 _func))
    {
        return Result::Failure;
    }

    Uint64 no_video_timeout = default_opts->no_video_timeout;
    if (!configSectionGetUint64 (section,
                                 opt_name__no_video_timeout,
                                 &no_video_timeout,
                                 no_video_timeout,
                                 _func))
    {
        return Result::Failure;
    }

    bool force_no_video_timeout = default_opts->force_no_video_timeout;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_no_video_timeout,
                                  &force_no_video_timeout,
                                  force_no_video_timeout,
                                  _func))
    {
        return Result::Failure;
    }

    Uint64 min_playlist_duration = default_opts->min_playlist_duration_sec;
    if (!configSectionGetUint64 (section,
                                 opt_name__min_playlist_duration,
                                 &min_playlist_duration,
                                 min_playlist_duration,
                                 _func))
    {
        return Result::Failure;
    }

    Uint64 pause_time = min_playlist_duration;
    if (!configSectionGetUint64 (section,
                                 opt_name__pause_time,
                                 &pause_time,
                                 pause_time,
                                 _func))
    {
        return Result::Failure;
    }

    ConstMemory push_uri = default_opts->push_uri->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__push_uri)) {
        if (opt->getValue())
            push_uri = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__push_uri, ": ", push_uri);

    ConstMemory push_username = default_opts->push_username->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__push_username)) {
        if (opt->getValue())
            push_username = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__push_username, ": ", push_username);

    ConstMemory push_password = default_opts->push_password->mem();
    if (MConfig::Option * const opt = section->getOption (opt_name__push_password)) {
        if (opt->getValue())
            push_password = opt->getValue()->mem();
    }
    logD_ (_func, opt_name__push_password, ": ", push_password);

    opts->channel_name  = st_grab (new (std::nothrow) String (channel_name));
    opts->channel_title = st_grab (new (std::nothrow) String (channel_title));
    opts->channel_desc  = st_grab (new (std::nothrow) String (channel_desc));

    opts->hidden = hidden;

    opts->keep_video_stream   = keep_video_stream;
    opts->continuous_playback = continuous_playback;

    opts->recording   = got_record_path;
    opts->record_path = st_grab (new (std::nothrow) String (record_path));

    opts->connect_on_demand = connect_on_demand;
    opts->connect_on_demand_timeout = connect_on_demand_timeout;

    opts->no_video_timeout = no_video_timeout;
    opts->force_no_video_timeout = force_no_video_timeout;
    opts->min_playlist_duration_sec = min_playlist_duration;
    opts->pause_time_sec = pause_time;

    opts->push_uri      = st_grab (new (std::nothrow) String (push_uri));
    opts->push_username = st_grab (new (std::nothrow) String (push_username));
    opts->push_password = st_grab (new (std::nothrow) String (push_password));

    return Result::Success;
}

Result
parseChannelItemConfig (MConfig::Section * const mt_nonnull section,
                        PlaybackItem     * const mt_nonnull default_item,
                        PlaybackItem     * const mt_nonnull item)
{
    char const opt_name__no_audio[]                  = "no_audio";
    char const opt_name__no_video[]                  = "no_video";
    char const opt_name__force_transcode[]           = "force_transcode";
    char const opt_name__force_transcode_audio[]     = "force_transcode_audio";
    char const opt_name__force_transcode_video[]     = "force_transcode_video";
    char const opt_name__mp3_passthrough[]           = "mp3_passthrough";
    char const opt_name__aac_perfect_timestamp[]     = "aac_perfect_timestamp";
    char const opt_name__sync_to_clock[]             = "sync_to_clock";
    char const opt_name__send_metadata[]             = "send_metadata";
    char const opt_name__drop_sei_nal_units[]        = "drop_sei_nal_units";
    char const opt_name__avc_config_interval[]       = "avc_config_interval";

    char const opt_name__chain[]                     = "chain";
    char const opt_name__uri[]                       = "uri";
    char const opt_name__fetch_uri[]                 = "fetch_uri";
    char const opt_name__playlist[]                  = "playlist";
    char const opt_name__playlist_conf[]             = "playlist_conf";
    char const opt_name__master[]                    = "master";

    char const opt_name__playlist_dir[]              = "dir";
    char const opt_name__dir_re_read[]               = "dir_re_read";

    char const opt_name__avtest[]                    = "avtest";
    char const opt_name__none[]                      = "none";

    bool no_audio = default_item->no_audio;
    if (!configSectionGetBoolean (section, opt_name__no_audio, &no_audio, no_audio, _func))
        return Result::Failure;
    logD_ (_func, opt_name__no_audio, ": ", no_audio);

    bool no_video = default_item->no_video;
    if (!configSectionGetBoolean (section, opt_name__no_video, &no_video, no_video, _func))
        return Result::Failure;
    logD_ (_func, opt_name__no_video, ": ", no_video);

    bool force_transcode = default_item->force_transcode;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode,
                                  &force_transcode,
                                  force_transcode,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode, ": ", force_transcode);

    bool force_transcode_audio = default_item->force_transcode_audio;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_audio,
                                  &force_transcode_audio,
                                  force_transcode_audio,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_audio, ": ", force_transcode_audio);

    bool force_transcode_video = default_item->force_transcode_video;
    if (!configSectionGetBoolean (section,
                                  opt_name__force_transcode_video,
                                  &force_transcode_video,
                                  force_transcode_video,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__force_transcode_video, ": ", force_transcode_video);

    bool mp3_passthrough = default_item->mp3_passthrough;
    if (!configSectionGetBoolean (section,
                                  opt_name__mp3_passthrough,
                                  &mp3_passthrough,
                                  mp3_passthrough,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__mp3_passthrough, ": ", mp3_passthrough);

    bool aac_perfect_timestamp = default_item->aac_perfect_timestamp;
    if (!configSectionGetBoolean (section,
                                  opt_name__aac_perfect_timestamp,
                                  &aac_perfect_timestamp,
                                  aac_perfect_timestamp,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__aac_perfect_timestamp, ": ", aac_perfect_timestamp);

    bool sync_to_clock = default_item->sync_to_clock;
    if (!configSectionGetBoolean (section,
                                  opt_name__sync_to_clock,
                                  &sync_to_clock,
                                  sync_to_clock,
                                  _func))
    {
        return Result::Failure;
    }

    bool sync_to_clock_specified = default_item->sync_to_clock_specified;
    if (!sync_to_clock_specified) {
        if (MConfig::Option * const opt = section->getOption (opt_name__sync_to_clock)) {
            if (opt->getBoolean() != MConfig::Boolean_Default)
                sync_to_clock_specified = true;
        }
    }

    logD_ (_func, opt_name__sync_to_clock, ": ", sync_to_clock, " (", (sync_to_clock_specified ? "explicit" : "implicit"), ")");

    bool send_metadata = default_item->send_metadata;
    if (!configSectionGetBoolean (section,
                                  opt_name__send_metadata,
                                  &send_metadata,
                                  send_metadata,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__send_metadata, ": ", send_metadata);

    bool drop_sei_nal_units = default_item->drop_sei_nal_units;
    if (!configSectionGetBoolean (section,
                                  opt_name__drop_sei_nal_units,
                                  &drop_sei_nal_units,
                                  drop_sei_nal_units,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__drop_sei_nal_units, ": ", drop_sei_nal_units);

    Uint64 avc_config_interval_millisec = default_item->avc_config_interval_millisec;
    if (!configSectionGetUint64 (section,
                                 opt_name__avc_config_interval,
                                 &avc_config_interval_millisec,
                                 avc_config_interval_millisec,
                                 _func))
    {
        return Result::Failure;
    }

    bool dir_re_read = default_item->dir_re_read;
    if (!configSectionGetBoolean (section,
                                  opt_name__dir_re_read,
                                  &dir_re_read,
                                  dir_re_read,
                                  _func))
    {
        return Result::Failure;
    }
    logD_ (_func, opt_name__dir_re_read, ": ", dir_re_read);

    PlaybackItem::SpecKind spec_kind = default_item->spec_kind;
    ConstMemory stream_spec = String::mem (default_item->stream_spec);
    Ref<MConfig::Section> playlist_section;
    {
        int num_set_opts = 0;

        if (MConfig::Option * const opt = section->getOption (opt_name__chain)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Chain;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__chain, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__uri)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Uri;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__uri, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__fetch_uri)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::FetchUri;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__fetch_uri, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__master)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::Slave;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__master, ": ", stream_spec);
        }

        if (MConfig::Section * const psect = section->getSection (opt_name__playlist)) {
            playlist_section = psect;
            spec_kind = PlaybackItem::SpecKind::PlaylistSection;
            ++num_set_opts;
            logD_ (_func, "playlist section");
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__playlist)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::PlaylistXml;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__playlist, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__playlist_conf)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::PlaylistConf;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__playlist_conf, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__playlist_dir)) {
            if (opt->getValue()) {
                stream_spec = opt->getValue()->mem();
                spec_kind = PlaybackItem::SpecKind::PlaylistDir;
                ++num_set_opts;
            }
            logD_ (_func, opt_name__playlist_dir, ": ", stream_spec);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__avtest)) {
            stream_spec = "avtest";
            spec_kind = PlaybackItem::SpecKind::Builtin;
            ++num_set_opts;
            logD_ (_func, opt_name__avtest);
        }

        if (MConfig::Option * const opt = section->getOption (opt_name__none)) {
            stream_spec = ConstMemory(); 
            spec_kind = PlaybackItem::SpecKind::None;
            ++num_set_opts;
            logD_ (_func, opt_name__none);
        }

        if (num_set_opts > 1)
            logW_ (_func, "only one of uri/fetch_uri/chain/master/playlist/dir/avtest/none should be specified");
    }

    {
        item->input_opts = grab (new (std::nothrow) InputOptions);

        ConstMemory input_section_name = "input";
        if (MConfig::Section * const input_section = section->getSection (input_section_name)) {
            if (logLevelOn_ (LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _func, "input:");
                input_section->dump (logs);
                logUnlock ();
            }

            // TODO merge width default_input
            item->input_opts->config_section = input_section;
        } else {
            // TODO default_input
            item->input_opts->config_section = grab (new (std::nothrow) MConfig::Section (input_section_name));
        }
    }

    item->stream_spec = st_grab (new (std::nothrow) String (stream_spec));
    item->spec_kind = spec_kind;
    item->playlist_section = playlist_section;

    item->no_audio = no_audio;
    item->no_video = no_video;

    item->force_transcode = force_transcode;
    item->force_transcode_audio = force_transcode_audio;
    item->force_transcode_video = force_transcode_video;

    item->mp3_passthrough = mp3_passthrough;

    item->aac_perfect_timestamp = aac_perfect_timestamp;

    item->sync_to_clock = sync_to_clock;
    item->sync_to_clock_specified = sync_to_clock_specified;

//#warning TODO send_metadata
    item->send_metadata = /* TODO send_metadata */ false;
    item->drop_sei_nal_units = drop_sei_nal_units;
    item->avc_config_interval_millisec = avc_config_interval_millisec;
    item->dir_re_read = dir_re_read;

    return Result::Success;
}

static Result
parseOutputConfig (MConfig::Section * const mt_nonnull section,
                   ChannelOptions   * const mt_nonnull default_opts,
                   OutputDesc       * const default_output_desc,
                   ChannelOptions   * const mt_nonnull opts,
                   bool               const is_default_channel)
{
    opts->output_opts = grab (new (std::nothrow) OutputOptions);

    if (default_opts->output_opts) {
        List< Ref<OutputDesc> >::iterator iter (default_opts->output_opts->output_list);
        while (!iter.done()) {
            Ref<OutputDesc> &output_desc = iter.next ()->data;

            if (logLevelOn_ (LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _func, "default channel output");
                output_desc->config_section->dump (logs);
                logUnlock ();
            }

            opts->output_opts->output_list.append (output_desc);
        }
    }

    MConfig::Section::iterator iter (*section);
    while (!iter.done()) {
        MConfig::SectionEntry * const sect_entry = iter.next ();
        if (sect_entry->getType() == MConfig::SectionEntry::Type_Section
            && equal (sect_entry->getName(), "output"))
        {
            MConfig::Section * const output_section = static_cast <MConfig::Section*> (sect_entry);

            Ref<MConfig::Section> const merged_section =
                    MConfig::mergeSections ((default_output_desc ? default_output_desc->config_section : NULL),
                                            output_section);

            if (logLevelOn_ (LogLevel::Debug)) {
                logLock ();
                log_locked__ (LogLevel::Debug, _func, "output:");
                merged_section->dump (logs);
                logUnlock ();
            }

            Ref<OutputDesc> const output_desc = grab (new (std::nothrow) OutputDesc);
            output_desc->config_section = merged_section;
            opts->output_opts->output_list.append (output_desc);
        }
    }

    if (!is_default_channel) {
        if (opts->output_opts->output_list.isEmpty()) {
            if (default_output_desc) {
                logD_ (_func, "No outputs, using default");
                opts->output_opts->output_list.append (default_output_desc);
            }
        }
    }

    return Result::Success;
}

static Result
parseDomainList (MConfig::Section * const mt_nonnull section,
                 DomainList       * const mt_nonnull domain_list)
{
    MConfig::Section::iterator iter (*section);
    while (!iter.done()) {
        MConfig::SectionEntry * const sect_entry = iter.next ();
        if (sect_entry->getType() == MConfig::SectionEntry::Type_Option) {
            domain_list->append (st_grab (new (std::nothrow) String (sect_entry->getName())));
            logD_ (_func, "domain: ", sect_entry->getName());
        }
    }

    return Result::Success;
}

Result
parseAllowedDomains (MConfig::Section * const section,
                     ChannelOptions   * const default_opts,
                     DomainList       * const mt_nonnull domain_list)
{
    if (default_opts && default_opts->domain_opts) {
        DomainList::iterator iter (default_opts->domain_opts->allowed_domains);
        while (!iter.done()) {
            StRef<String> &domain = iter.next ()->data;
            domain_list->append (domain);
        }
    }

    if (!section)
        return Result::Success;

    MConfig::Section * const domains_section = section->getSection ("domains");
    if (!domains_section)
        return Result::Success;

    return parseDomainList (domains_section, domain_list);
}

Result
parseChannelConfig (MConfig::Section * const mt_nonnull section,
                    ConstMemory        const config_item_name,
                    bool               const is_default_channel,
                    ChannelOptions   * const mt_nonnull default_opts,
                    OutputDesc       * const default_output_desc,
                    ChannelOptions   * const mt_nonnull opts,
                    PlaybackItem     * const mt_nonnull item)
{
    opts->domain_opts = grabNew <DomainOptions> ();
    if (   !parseChannelOptionsConfig (section, config_item_name, default_opts, opts)
        || !parseChannelItemConfig    (section, default_opts->default_item, item)
        || !parseOverlayConfig        (section, opts)
        || !parseOutputConfig         (section, default_opts, default_output_desc, opts, is_default_channel)
        || !parseAllowedDomains       (section, default_opts, &opts->domain_opts->allowed_domains))
    {
        return Result::Failure;
    }

    return Result::Success;
}

Result
parseRatelimitConfig (MConfig::Section * const mt_nonnull section,
                      RateLimitParams  * const mt_nonnull params)
{
    if (   !configSectionGetBoolean_ (section, "realtime",           &params->realtime,                    _func)
        || !configSectionGetUint64_  (section, "min_frame_duration", &params->min_frame_duration_microsec, _func)
        || !configSectionGetUint64_  (section, "bytes_per_second",   &params->bytes_per_second,            _func)
        || !configSectionGetUint64_  (section, "burst_duration",     &params->burst_duration_microsec,     _func)
        || !configSectionGetUint64_  (section, "max_burst_size",     &params->max_burst_size,              _func)
        || !configSectionGetUint64_  (section, "prepush_duration",   &params->prepush_duration_microsec,   _func)
        || !configSectionGetUint64_  (section, "prepush_size",       &params->prepush_size,                _func))
    {
        return Result::Failure;
    }

    return Result::Success;
}

void configWarnNoEffect (ConstMemory const opt_name)
{
    logI_ (_func, "changing \"", opt_name, "\" option has no effect until the server is restarted");
}

}

