/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/playback_item.h>


namespace Moment {

static ConstMemory boolToStr (bool b)
{
    return b ? ConstMemory ("y") : ConstMemory ("n");
}

void
PlaybackItem::toConfigStringPages (PagePool               * mt_nonnull const page_pool,
                                   PagePool::PageListInfo * mt_nonnull const page_list)
{
    PagePool::PageListOutputStream out (page_pool, page_list);

    if (input_opts)
        input_opts->config_section->dump (&out);

    switch (spec_kind.val()) {
        case SpecKind::None:
            break;
        case SpecKind::Chain:
            out.print ("chain = \"", stream_spec, "\"\n");
            break;
        case SpecKind::Uri:
            out.print ("uri = \n", stream_spec, "\"\n");
            break;
        case SpecKind::FetchUri:
            out.print ("fetch_uri = \n", stream_spec, "\"\n");
            break;
        case SpecKind::Slave:
        case SpecKind::PlaylistSection:
        case SpecKind::PlaylistXml:
        case SpecKind::PlaylistConf:
        case SpecKind::PlaylistDir:
            break;
        case SpecKind::Builtin:
            if (equal (String::mem (stream_spec), "avtest")) {
                out.print ("avtest\n");
            }
            break;
    }

    out.print ("no_audio = ", boolToStr (no_audio), "\n");
    out.print ("no_video = ", boolToStr (no_video), "\n");

    out.print ("force_transcode = ", boolToStr (force_transcode), "\n");
    out.print ("force_transcode_audio = ", boolToStr (force_transcode_audio), "\n");
    out.print ("force_transcode_video = ", boolToStr (force_transcode_video), "\n");

    out.print ("mp3_passthrough = ", boolToStr (mp3_passthrough), "\n");

    out.print ("aac_perfect_timestamp = ", boolToStr (aac_perfect_timestamp), "\n");

    if (sync_to_clock_specified)
        out.print ("sync_to_clock = ", boolToStr (sync_to_clock), "\n");

    out.print ("send_metadata = ", boolToStr (send_metadata), "\n");
    out.print ("drop_sei_nal_units = ", boolToStr (drop_sei_nal_units), "\n");

    out.print ("avc_config_interval = ", avc_config_interval_millisec, "\n");

    out.flush ();
}

}

