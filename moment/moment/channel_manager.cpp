/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>
#include <cctype>

#include <mconfig/mconfig.h>

#include <moment/util_config.h>

#include <moment/channel_manager.h>


using namespace M;

namespace Moment {

HttpManager::HttpRequestHandler ChannelManager::admin_http_handler = {
    adminHttpRequest
};

HttpManager::HttpRequestResult
ChannelManager::adminHttpRequest (HttpRequest * const mt_nonnull req,
                                  IpAddress     const /* local_addr */,
                                  Sender      * const mt_nonnull conn_sender,
                                  Memory        const /* msg_body */,
                                  void        * const _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "reload_channel"))
    {
        // TODO get channel name attribute

        logD_ (_func, "reload_channel");

        ConstMemory const item_name = req->getParameter ("conf_file");
        if (item_name.len() == 0) {
            ConstMemory const reply_body = "400 Bad Request: no conf_file parameter";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__400_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("cman 400 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        ConstMemory const dir_name = self->confd_dirname->mem();
        StRef<String> const path = makeString (dir_name, "/", item_name);

        if (!self->loadConfigItem (item_name, path->mem())) {
            ConstMemory const reply_body = "500 Internal Server Error: loadConfigItem() failed";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__500_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("cman 500 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        ConstMemory const reply_body = "OK";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/plain", reply_body.len()),
                           "\r\n",
                           reply_body);
        logA_ ("cman OK ", req->getClientAddress(), " ", req->getRequestLine());
    } else
    if (req->getNumPathElems() >= 2
        && (equal (req->getPath (1), "update_playlist") ||
            equal (req->getPath (1), "update_playlist_now")))
    {
        bool const keep_cur_item = equal (req->getPath (1), "update_playlist");

        ConstMemory channel_name;
        if (req->getNumPathElems() >= 3)
            channel_name = req->getPath (2);
        else
            channel_name = req->getParameter ("name");

        StRef<String> err_msg;
        if (!self->updatePlaylist (channel_name, keep_cur_item, &err_msg)) {
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__500_HEADERS (String::len (err_msg)),
                               "\r\n",
                               String::mem (err_msg));
            logA_ ("cman 500 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        ConstMemory const reply_body = "OK";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/plain", reply_body.len()),
                           "\r\n",
                           reply_body);
        logA_ ("cman OK ", req->getClientAddress(), " ", req->getRequestLine());
    } else
    if (req->getNumPathElems() >= 2
        && (equal (req->getPath (1), "set_position") ||
            equal (req->getPath (1), "set_position_id")))
    {
        bool const item_name_is_id = equal (req->getPath (1), "set_position_id");

        ConstMemory channel_name;
        ConstMemory item_name;
        ConstMemory seek_str;
        if (req->getNumPathElems() >= 5) {
            channel_name = req->getPath (2);
            item_name    = req->getPath (3);
            seek_str     =  req->getPath (4);
        } else {
            channel_name = req->getParameter ("name");
            item_name    = req->getParameter ("item");
            seek_str     = req->getParameter ("seek");
        }

        if (!self->setPosition (channel_name, item_name, item_name_is_id, seek_str)) {
            ConstMemory const reply_body = "error";
            conn_sender->send (self->page_pool,
                               true /* do_flush */,
                               MOMENT_SERVER__500_HEADERS (reply_body.len()),
                               "\r\n",
                               reply_body);
            logA_ ("cman 500 ", req->getClientAddress(), " ", req->getRequestLine());
            goto _return;
        }

        ConstMemory const reply_body = "OK";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__OK_HEADERS ("text/plain", reply_body.len()),
                           "\r\n",
                           reply_body);
        logA_ ("cman OK ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
        return HttpManager::HttpRequestResult::NotFound;
    }

_return:
    if (!req->getKeepalive())
	conn_sender->closeAfterFlush ();

    return HttpManager::HttpRequestResult::Success;
}

HttpManager::HttpRequestHandler ChannelManager::server_http_handler = {
    serverHttpRequest
};

HttpManager::HttpRequestResult
ChannelManager::serverHttpRequest (HttpRequest * const mt_nonnull req,
                                   IpAddress     const local_addr,
                                   Sender      * const mt_nonnull conn_sender,
                                   Memory        const /* msg_body */,
                                   void        * const _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "playlist.json")
        && self->serve_playlist_json)
    {
        logD_ (_func, "playlist.json");

        PagePool::PageListInfo page_list;

        static char const prefix [] = "[\n";
        static char const suffix [] = "]\n";

        self->page_pool->getFillPages (&page_list, prefix);

        self->mutex.lock ();
        {
            StRef<String> const rtmp_addr  = self->config_manager->getRtmpAddrStr  (req->getHost(), local_addr);
            StRef<String> const rtmpt_addr = self->config_manager->getRtmptAddrStr (req->getHost(), local_addr);

            bool use_rtmpt_proto = false;
            if (equal (self->playlist_json_protocol->mem(), "rtmpt"))
                use_rtmpt_proto = true;

            typedef AvlTree< ConfigItem*,
                             MemberExtractor< ConfigItem,
                                              StRef<String>,
                                              &ConfigItem::channel_name,
                                              Memory,
                                              AccessorExtractor< String,
                                                                 Memory,
                                                                 &String::mem > >,
                             MemoryComparator<> >
                    EntryTree;
            EntryTree entry_tree;
            {
                ItemHash::iterator iter (self->item_hash);
                while (!iter.done()) {
                    ConfigItem * const item = iter.next ()->ptr();

                    if (item->channel->getChannelOptions()->hidden)
                        continue;

                    entry_tree.add (item);
                }
            }
            {
                EntryTree::bl_iterator iter (entry_tree);
                while (!iter.done()) {
                    ConfigItem * const item = iter.next ()->value;

                    StRef<String> const channel_line = makeString (
                            "[ \"", item->channel_title, "\", "
                            "\"", (use_rtmpt_proto ? ConstMemory ("rtmpt://") : ConstMemory ("rtmp://")),
                                    (use_rtmpt_proto ? rtmpt_addr->mem() : rtmp_addr->mem()),
                                    "/live\", "
                            "\"", item->channel_name->mem(), "\" ],\n");

                    self->page_pool->getFillPages (&page_list, channel_line->mem());

                    logD_ (_func, "playlist.json line: ", channel_line->mem());
                }
            }
	}
        self->mutex.unlock ();

        self->page_pool->getFillPages (&page_list, suffix);

	conn_sender->send (self->page_pool,
			   false /* do_flush */,
			   MOMENT_SERVER__OK_HEADERS ("text/plain", page_list.data_len),
			   "\r\n");
	conn_sender->sendPages (self->page_pool, page_list.first, /*msg_offs=*/ 0, page_list.data_len, true /* do_flush */);

	logA_ ("cman 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
        return HttpManager::HttpRequestResult::NotFound;
    }

    if (!req->getKeepalive())
	conn_sender->closeAfterFlush ();

    return HttpManager::HttpRequestResult::Success;
}

void
ChannelManager::notifyChannelCreated (ChannelInfo * const mt_nonnull channel_info)
{
    logD_ (_func_);

    {
        ChannelCreationMessage * const msg = new (std::nothrow) ChannelCreationMessage;
        assert (msg);
        msg->channel        = channel_info->channel;
        msg->channel_name   = newString (channel_info->channel_name);
        msg->config_section = channel_info->config_section;

        mutex.lock ();
        channel_creation_messages.append (msg);
        mutex.unlock ();
    }

    deferred_reg.scheduleTask (&channel_created_task, false /* permanent */);
}

bool
ChannelManager::channelCreatedTask (void * const _self)
{
    ChannelManager * const self = static_cast <ChannelManager*> (_self);

    logD_ (_func_);

    self->mutex.lock ();

    while (!self->channel_creation_messages.isEmpty()) {
        ChannelCreationMessage * const msg = self->channel_creation_messages.getFirst();
        self->channel_creation_messages.remove (msg);

        ChannelInfo channel_info;
        channel_info.channel        = msg->channel;
        channel_info.channel_name   = msg->channel_name->mem();
        channel_info.config_section = msg->config_section;

        self->mutex.unlock ();

        self->fireChannelCreated (&channel_info);
        delete msg;

        self->mutex.lock ();
    }

    self->mutex.unlock ();

    return false /* do not rechedule */;
}

Result
ChannelManager::loadStreamsSection ()
{
    logD_ (_func_);

    Ref<MomentServer> const moment = weak_moment.getRef();
    if (!moment) {
        logD_ (_func, "moment gone");
        return Result::Failure;
    }

    Ref<MConfig::Config> const config = moment->getConfigManager()->getConfig();

    if (MConfig::Section * const default_output_section = config->getSection ("default_output")) {
        default_output_desc = grabNew <OutputDesc> ();
        default_output_desc->config_section = default_output_section;
    }

    Ref<PlaybackItem> const default_item = grabNew <PlaybackItem> ();
    default_channel_opts = grabNew <ChannelOptions> ();
    default_channel_opts->default_item = default_item;
    if (MConfig::Section * const default_stream_section = config->getSection ("default_stream")) {
        parseChannelConfig (default_stream_section,
                            ConstMemory() /* config_item_name */,
                            true          /* is_default_channel */,
                            default_channel_opts,
                            default_output_desc,
                            default_channel_opts,
                            default_item);
    }

    if (logLevelOn_ (LogLevel::Debug)) {
        default_channel_opts->dump (LogLevel::Debug, _func, "default_channel_opts: ");
        default_item->dump (LogLevel::Debug, _func, "default_item: ");
    }

    if (MConfig::Section * const streams_section = config->getSection ("streams")) {
        MConfig::Section::iterator streams_iter (*streams_section);
        while (!streams_iter.done()) {
            MConfig::SectionEntry * const entry = streams_iter.next ();
            if (entry->getType() == MConfig::SectionEntry::Type_Section) {
                MConfig::Section * const section = static_cast <MConfig::Section*> (entry);
                if (!parseConfigItem (moment, section->getName(), config, section))
                    return Result::Failure;
            }
        }
    }

    return Result::Success;
}

Result
ChannelManager::loadConfdDir ()
{
    logD_ (_func_);

    ConstMemory const dir_name = confd_dirname->mem();
    StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (dir_name);

    StRef<Vfs::VfsDirectory> const dir = vfs->openDirectory ("");
    if (!dir) {
        logE_ (_func, "could not open ", dir_name, " directory: ", exc->toString());
        return Result::Success;
    }

    for (;;) {
        StRef<String> filename;
        if (!dir->getNextEntry (filename)) {
            logE_ (_func, "Vfs::VfsDirectory::getNextEntry() failed: ", exc->toString());
            return Result::Failure;
        }
        if (!filename)
            break;

        if (equal (filename->mem(), ".") || equal (filename->mem(), ".."))
            continue;

        // Don't process hidden files (e.g. vim tmp files).
        if (filename->len() > 0 && filename->mem().mem() [0] == '.')
            continue;

        // TODO Handle .__moment_old and .__moment_new suffixes.

        StRef<String> const path = makeString (dir_name, "/", filename->mem());
        if (!loadConfigItem (filename->mem(), path->mem()))
            return Result::Failure;
    }

    return Result::Success;
}

Result
ChannelManager::loadConfigFull ()
{
    logD_ (_func_);

    Result res = Result::Success;
    // default_channel_opts are initialized in loadStreamsSection()
    res = loadStreamsSection () ? res : Result (Result::Failure);
    res = loadConfdDir ()       ? res : Result (Result::Failure);
    return res;
}

Result
ChannelManager::parseConfigItem (MomentServer     * const mt_nonnull moment,
                                 ConstMemory        const item_name,
                                 MConfig::Config  * const mt_nonnull config,
                                 MConfig::Section * const mt_nonnull section)
{
    logD_ (_func, "item_name: ", item_name);

    mutex.lock ();

    StRef<ConfigItem> item;
    if (ItemHash::EntryKey const old_item_key = item_hash.lookup (item_name)) {
        item = old_item_key.getData();
        item->channel->getPlayback()->restart ();
    } else {
        item = st_grabNew <ConfigItem> ();
        item_hash.add (item_name, item);
    }

    Ref<ChannelOptions> const channel_opts = grabNew <ChannelOptions> ();
    *channel_opts = *default_channel_opts;

    channel_opts->default_item = grabNew <PlaybackItem> ();
    *channel_opts->default_item = *default_channel_opts->default_item;

    Ref<PlaybackItem> const playback_item = grabNew <PlaybackItem> ();
    *playback_item = *channel_opts->default_item;

    parseChannelConfig (section,
                        item_name,
                        false /* is_default_channel */,
                        default_channel_opts,
                        default_output_desc,
                        channel_opts,
                        playback_item);
    item->channel_name  = newString (channel_opts->channel_name->mem());
    item->channel_title = newString (channel_opts->channel_title->mem());

    if (logLevelOn_ (LogLevel::Debug)) {
        channel_opts->dump (LogLevel::Debug, _func, "channel_opts: ");
        playback_item->dump (LogLevel::Debug, _func, "playback_item: ");
    }

//#warning Do not substitute existing channel.
//#warning Notify only when a new channel is created.
    item->channel = grabNewObject <Channel> ();
    item->channel->init (moment, channel_opts);

    item->config = config;
    item->config_section = section;

    item->push_agent = NULL;
    if (!String::isNullString (channel_opts->push_uri)) {
        Ref<PushProtocol> const push_protocol = moment->getSourceManager()->getPushProtocolForUri (channel_opts->push_uri->mem());
        if (push_protocol) {
            item->push_agent = grabNewObject <PushAgent> ();
            item->push_agent->init (moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor(),
                                    moment->getStreamManager(),
                                    item->channel_name->mem(),
                                    push_protocol,
                                    String::mem (channel_opts->push_uri),
                                    String::mem (channel_opts->push_username),
                                    String::mem (channel_opts->push_password));
        }
    }

    // Calling with 'mutex' locked for extra safety.
    switch (playback_item->spec_kind.val()) {
        case PlaybackItem::SpecKind::PlaylistSection: {
            if (!item->channel->getPlayback()->loadPlaylistSection (
                        playback_item->playlist_section,
                        false /* keep_cur_item */,
                        playback_item))
            {
                logE_ (_func, "Could not parse playlist section for channel ", String::mem (channel_opts->channel_name));
            }
        } break;
        case PlaybackItem::SpecKind::PlaylistXml:
        case PlaybackItem::SpecKind::PlaylistConf: {
            StRef<String> err_msg;
            if (!item->channel->getPlayback()->loadPlaylistFile (
                        playback_item->stream_spec->mem(),
                        (playback_item->spec_kind == PlaybackItem::SpecKind::PlaylistXml),
                        false /* keep_cur_item */,
                        playback_item,
                        &err_msg))
            {
                logE_ (_func, "Could not parse playlist file \"", playback_item->stream_spec, "\":\n", err_msg);
            }
        } break;
        case PlaybackItem::SpecKind::PlaylistDir: {
            if (!item->channel->getPlayback()->loadPlaylistDirectory (playback_item->stream_spec->mem(),
                                                                      playback_item->dir_re_read,
                                                                      false /* keep_cur_item */,
                                                                      playback_item))
            {
                logE_ (_func, "Could not create playlist for directory \"", playback_item->stream_spec, "\"");
            }
        } break;
        case PlaybackItem::SpecKind::Chain:
        case PlaybackItem::SpecKind::Uri:
        case PlaybackItem::SpecKind::FetchUri:
        case PlaybackItem::SpecKind::Slave:
        case PlaybackItem::SpecKind::Builtin: {
            item->channel->getPlayback()->setSingleItem (playback_item);
        } break;
        default:
            logW_ (_func, "Unknown playback item spec kind: ", playback_item->spec_kind);
    }

    mutex.unlock ();

    {
        ChannelInfo channel_info;
        channel_info.channel        = item->channel;
        channel_info.channel_name   = item->channel_name->mem();
        channel_info.config_section = item->config_section;

        notifyChannelCreated (&channel_info);
    }

    return Result::Success;
}

Result
ChannelManager::loadConfigItem (ConstMemory const item_name,
                                ConstMemory const item_path)
{
    logD_ (_func, "item_name: ", item_name, ", item_path: ", item_path);

    Ref<MomentServer> const moment = weak_moment.getRef();
    if (!moment) {
        logD_ (_func, "moment gone");
        return Result::Failure;
    }

    Ref<MConfig::Config> const config = grabNewObject <MConfig::Config> ();
    if (!MConfig::parseConfig (item_path, config)) {
        logE_ (_func, "could not parse config file ", item_path);

        mutex.lock ();
        if (ItemHash::EntryKey const old_item_key = item_hash.lookup (item_name)) {
            StRef<ConfigItem> const item = old_item_key.getData();
            item->channel->getPlayback()->restart ();
            item_hash.remove (old_item_key);
        }
        mutex.unlock ();

        return Result::Failure;
    }

    return parseConfigItem (moment, item_name, config, config->getRootSection());
}

Result
ChannelManager::updatePlaylist (ConstMemory     const channel_name,
                                bool            const keep_cur_item,
                                StRef<String> * const mt_nonnull ret_err_msg)
{
    mutex.lock ();

    ItemHash::EntryKey const entry_key = item_hash.lookup (channel_name);
    if (!entry_key) {
        mutex.unlock ();
        StRef<String> const err_msg = makeString ("channel not found: ", channel_name);
        logE_ (_func, err_msg);
        *ret_err_msg = err_msg;
        return Result::Failure;
    }

    ConfigItem * const item = entry_key.getData();
    PlaybackItem * const playback_item = item->channel->getChannelOptions()->default_item;

    if (   playback_item->spec_kind != PlaybackItem::SpecKind::PlaylistSection
        && playback_item->spec_kind != PlaybackItem::SpecKind::PlaylistXml
        && playback_item->spec_kind != PlaybackItem::SpecKind::PlaylistConf)
    {
        mutex.unlock ();
        StRef<String> const err_msg = makeString ("no playlist for channel \"", channel_name, "\"");
        logE_ (_func, err_msg);
        *ret_err_msg = err_msg;
        return Result::Failure;
    }

    if (playback_item->spec_kind == PlaybackItem::SpecKind::PlaylistSection) {
        if (!item->channel->getPlayback()->loadPlaylistSection (
                    playback_item->playlist_section,
                    keep_cur_item,
                    playback_item))
        {
            mutex.unlock ();
            logE_ (_func, "loadPlaylistSection() failed, channel \"", channel_name, "\"");
            *ret_err_msg = NULL;
            return Result::Failure;
        }
    } else {
        StRef<String> err_msg;
        if (!item->channel->getPlayback()->loadPlaylistFile (
                    String::mem (playback_item->stream_spec),
                    (playback_item->spec_kind == PlaybackItem::SpecKind::PlaylistXml),
                    keep_cur_item,
                    playback_item,
                    &err_msg))
        {
            mutex.unlock ();
            logE_ (_func, "loadPlaylistFile() failed, channel \"", channel_name, "\": ", err_msg);
            *ret_err_msg = makeString (String::mem (err_msg));
            return Result::Failure;
        }
    }

    mutex.unlock ();

    return Result::Success;
}

Result
ChannelManager::setPosition (ConstMemory const channel_name,
                             ConstMemory const item_name,
                             bool        const item_name_is_id,
                             ConstMemory const seek_str)
{
    Time seek;
    if (!parseDuration (seek_str, &seek)) {
        logE_ (_func, "couldn't parse seek time: ", seek_str);
        return Result::Failure;
    }

    mutex.lock ();

    ItemHash::EntryKey const entry_key = item_hash.lookup (channel_name);
    if (!entry_key) {
        mutex.unlock ();
        logE_ (_func, "channel not found: ", channel_name);
        return Result::Failure;
    }

    ConfigItem * const item = entry_key.getData();

    Result res;
    if (item_name_is_id) {
        res = item->channel->getPlayback()->setPosition_Id (item_name, seek);
    } else {
        Uint32 item_idx;
        if (!strToUint32_safe (item_name, &item_idx)) {
            mutex.unlock ();
            logE_ (_func, "failed to parse item index");
            return Result::Failure;
        }

        res = item->channel->getPlayback()->setPosition_Index (item_idx, seek);
    }

    if (!res) {
        mutex.unlock ();
        logE_ (_func, "item not found: ", item_name,
               (item_name_is_id ? ConstMemory (" (id)") : ConstMemory (" (idx)")), ", channel: ", channel_name);
        return Result::Failure;
    }

    mutex.unlock ();

    return Result::Success;
}

void
ChannelManager::fillChannelInfo (ConfigItem  * const mt_nonnull item,
                                 ChannelInfo * const mt_nonnull ret_info)
{
    ret_info->channel        = item->channel;
    ret_info->channel_name   = item->channel_name->mem();
    ret_info->config_section = item->config_section;
}

mt_mutex (mutex) Result
ChannelManager::getChannelInfo_locked (ConstMemory   const channel_name,
                                       ChannelInfo * const mt_nonnull ret_info)
{
    ItemHash::EntryKey const entry_key = item_hash.lookup (channel_name);
    if (!entry_key)
        return Result::Failure;

    ConfigItem * const item = entry_key.getData();
    fillChannelInfo (item, ret_info);

    return Result::Success;
}

Result
ChannelManager::renameChannel (ConstMemory const old_name,
                               ConstMemory const new_name)
{
    StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (confd_dirname);

    if (!vfs->rename (old_name, new_name)) {
        logE_ (_func, "vfs.rename() failed: ", exc->toString());
        return Result::Failure;
    }

    loadConfigItem (old_name, makeString (confd_dirname, "/", old_name));
    loadConfigItem (new_name, makeString (confd_dirname, "/", new_name));

    return Result::Success;
}

Result
ChannelManager::saveChannel (ConstMemory const channel_name,
                             ConstMemory const config_mem)
{
    StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (confd_dirname);

    StRef<String> const old_tmp_filename = makeString (channel_name, ".__moment_old");
    StRef<String> const new_tmp_filename = makeString (channel_name, ".__moment_new");

    vfs->rename (channel_name, old_tmp_filename);

    {
        StRef<Vfs::VfsFile> const file = vfs->openFile (new_tmp_filename,
                                                        FileOpenFlags::Create | FileOpenFlags::Truncate,
                                                        FileAccessMode::WriteOnly);
        if (!file) {
            logE_ (_func, "vfs.openFile(\"", new_tmp_filename, "\") failed: ", exc->toString());
            return Result::Failure;
        }

        if (!file->getFile()->print (config_mem)) {
            logE_ (_func, "file.print() failed: ", exc->toString());
            return Result::Failure;
        }

        if (!file->getFile()->flush ()) {
            logE_ (_func, "file.flush() failed: ", exc->toString());
            return Result::Failure;
        }

        if (!file->getFile()->close ()) {
            logE_ (_func, "file.close() failed: ", exc->toString());
            return Result::Failure;
        }
    }

    vfs->rename (new_tmp_filename, channel_name);
    vfs->removeFile (old_tmp_filename);

    if (!loadConfigItem (channel_name, makeString (confd_dirname, "/", channel_name))) {
        logE_ (_func, "loadConfigItem() failed");
        return Result::Failure;
    }

    return Result::Success;
}

Result
ChannelManager::removeChannel (ConstMemory const channel_name)
{
    StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (confd_dirname);

    if (!vfs->removeFile (channel_name))
        logD_ (_func, "vfs.removeFile(\"", channel_name, "\") failed: ", exc->toString());

    if (!loadConfigItem (channel_name, makeString (confd_dirname, "/", channel_name)))
        logD_ (_func, "loadConfigItem(\"", channel_name, "\") failed");

    return Result::Success;
}

mt_const void
ChannelManager::init (MomentServer * const mt_nonnull moment,
                      PagePool     * const mt_nonnull page_pool)
{
    this->weak_moment = moment;
    this->config_manager = moment->getConfigManager();
    this->page_pool = page_pool;

    {
        Ref<MConfig::Config> const config = config_manager->getConfig();
        confd_dirname = makeString (config->getString_default ("moment/confd_dir",
                  #if defined LIBMARY_PLATFORM_WIN32
                    "..\\conf.d"
                  #elif defined LIBMARY_PLATFORM_MACOSX
                    "/Applications/MomentVideoServer.app/conf.d"
                  #else
                    "/opt/moment/conf.d"
                  #endif
                    ));
        if (confd_dirname->len() == 0)
            confd_dirname = makeString (".");

        {
            ConstMemory const opt_name = "moment/playlist_json";
            MConfig::BooleanValue const val = config->getBoolean (opt_name);
            logI_ (_func, opt_name, ": ", config->getString (opt_name));
            if (val == MConfig::Boolean_Invalid)
                logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));

            if (val == MConfig::Boolean_False)
                serve_playlist_json = false;
            else
                serve_playlist_json = true;
        }

        {
            ConstMemory const opt_name = "moment/playlist_json_protocol";
            ConstMemory opt_val = config->getString (opt_name);
            if (opt_val.len() == 0)
                opt_val = "rtmp";

            StRef<String> val_lowercase = newString (opt_val);
            Byte * const buf = val_lowercase->mem().mem();
            for (Size i = 0, i_end = val_lowercase->len(); i < i_end; ++i)
                buf [i] = (Byte) tolower (buf [i]);

            if (!equal (val_lowercase->mem(), "rtmpt"))
                val_lowercase = newString ("rtmp");

            logI_ (_func, opt_name, ": ", val_lowercase->mem());

            playlist_json_protocol = val_lowercase;
        }
    }

    deferred_reg.setDeferredProcessor (
            moment->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());

    moment->getHttpManager()->addAdminRequestHandler (
            CbDesc<HttpManager::HttpRequestHandler> (&admin_http_handler, this, this));
    moment->getHttpManager()->addServerRequestHandler (
            CbDesc<HttpManager::HttpRequestHandler> (&server_http_handler, this, this));
}

ChannelManager::ChannelManager (EmbedContainer * const embed_container)
    : Object              (embed_container),
      event_informer      (this /* outer_object */, &mutex),
      serve_playlist_json (true)
{
    channel_created_task.cb = CbDesc<DeferredProcessor::TaskCallback> (channelCreatedTask, this, this);
}

ChannelManager::~ChannelManager ()
{
    deferred_reg.release ();

    {
        ChannelCreationMessageList::iterator iter (channel_creation_messages);
        while (!iter.done()) {
            ChannelCreationMessage * const msg = iter.next ();
            delete msg;
        }
        channel_creation_messages.clear ();
    }
}

}

