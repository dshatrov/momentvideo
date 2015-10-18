/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/inc.h>

#include <moment-nvr/moment_nvr_module.h>


namespace MomentNvr {

StRef<String>
MomentNvrModule::channelStateToJson (ChannelRecorder::ChannelState * const mt_nonnull channel_state,
                                     ConstMemory const seq)
{
    return makeString ("{ "
                       "\"seq\": \"", seq, "\", "
                       "\"recording\": ", channel_state->recording, ", "
                       "\"online\": ",    channel_state->source_online, " "
                       "}");
}

GetFileSession::Frontend const MomentNvrModule::get_file_session_frontend = {
    getFileSession_done
};

struct MomentNvrModule::GetFileSession_Data : public Referenced
{
    mt_const MomentNvrModule *moment_nvr_module;
    mt_const WeakRef<Sender> weak_sender;
    mt_const Cb<HttpService::UnblockInputCallback> unblock_input_cb;
    mt_const bool keepalive;
    mt_mutex (mutex) GetFileSessionList::Element *list_el;
};

void
MomentNvrModule::getFileSession_done (Result   const /* res */,
                                      void   * const _data)
{
    GetFileSession_Data * const data = static_cast <GetFileSession_Data*> (_data);
    MomentNvrModule * const self = data->moment_nvr_module;

    logD_ (_func_);

    self->mutex.lock ();
    if (!data->list_el) {
        self->mutex.unlock ();
        return;
    }

    self->get_file_sessions.remove (data->list_el);
    data->list_el = NULL;
    self->mutex.unlock ();

    data->unblock_input_cb.call_ ();

    if (!data->keepalive) {
        Ref<Sender> const sender = data->weak_sender.getRef ();
        if (sender)
            sender->closeAfterFlush ();
    }
}

HttpService::HttpHandler const MomentNvrModule::http_handler =
{
    httpRequest,
    NULL /* httpMessageBody */
};

Result
MomentNvrModule::httpRequest (HttpRequest  * const mt_nonnull req,
                              HttpService::HttpConnectionInfo * const mt_nonnull conn_info,
                              IpAddress      const /* local_addr */,
                              Sender       * const mt_nonnull sender,
                              Memory         const /* msg_body */,
                              void        ** const mt_nonnull /* ret_msg_data */,
                              void         * const _self)
{
    MomentNvrModule * const self = static_cast <MomentNvrModule*> (_self);

    logD_ (_func, req->getRequestLine());

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "unixtime"))
    {
        self->doHttpRequest_unixtime (req, sender);
    } else
    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "channel_state"))
    {
        self->doHttpRequest_channel_state (req, sender);
    } else
    if (req->getNumPathElems() >= 2
        && (   equal (req->getPath (1), "file")
            || stringHasSuffix (req->getPath (1), ".mp4", NULL /* ret_str */)))
    {
        if (!self->doHttpRequest_file (req, conn_info, sender))
            return Result::Success;
    } else {
        logE_ (_func, "Unknown request: ", req->getFullPath());
        self->finishHttpRequest_NotFound (req, sender);
    }

    if (!req->getKeepalive())
        sender->closeAfterFlush ();

    return Result::Success;
}

void
MomentNvrModule::doHttpRequest_unixtime (HttpRequest * const mt_nonnull req,
                                         Sender      * const mt_nonnull sender)
{
    MOMENT_SERVER__HEADERS_DATE

    StRef<String> const unixtime_str = makeString (getUnixtime());
    sender->send (page_pool,
                  true /* do_flush */,
                  MOMENT_SERVER__OK_HEADERS ("text/html", unixtime_str->len()),
                  "\r\n",
                  unixtime_str);

    logA_ ("mod_nvr OK ", req->getClientAddress(), " ", req->getRequestLine());
}

void
MomentNvrModule::doHttpRequest_channel_state (HttpRequest * const mt_nonnull req,
                                              Sender      * const mt_nonnull sender)
{
    ConstMemory const channel_name = req->getParameter ("stream");
    ConstMemory const seq = req->getParameter ("seq");

    ChannelRecorder::ChannelState channel_state;
    ChannelRecorder::ChannelResult const res =
            channel_recorder->getChannelState (channel_name, &channel_state);
    if (res == ChannelRecorder::ChannelResult_ChannelNotFound) {
        finishHttpRequest_NotFound (req, sender);
        return;
    } else
    if (res == ChannelRecorder::ChannelResult_Failure) {
        finishHttpRequest_InternalServerError (req, sender);
        return;
    }
    assert (res == ChannelRecorder::ChannelResult_Success);

    MOMENT_SERVER__HEADERS_DATE

    StRef<String> const reply_body = channelStateToJson (&channel_state, seq);
    sender->send (page_pool,
                       true /* do_flush */,
                       MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
                       "\r\n",
                       reply_body->mem());

    logA_ ("mod_nvr OK ", req->getClientAddress(), " ", req->getRequestLine());
}

bool
MomentNvrModule::doHttpRequest_file (HttpRequest                     * const mt_nonnull req,
                                     HttpService::HttpConnectionInfo * const mt_nonnull conn_info,
                                     Sender                          * const mt_nonnull sender)
{
    ConstMemory const channel_name = req->getParameter ("stream");

    Uint64 start_unixtime_sec = 0;
    if (!strToUint64_safe (req->getParameter ("start"), &start_unixtime_sec, 10 /* base */)) {
        logE_ (_func, "Bad \"start\" request parameter value");
        finishHttpRequest_BadRequest (req, sender);
        return true;
    }

    Uint64 duration_sec = 0;
    if (!strToUint64_safe (req->getParameter ("duration"), &duration_sec, 10 /* base */)) {
        logE_ (_func, "Bad \"duration\" request parameter value");
        finishHttpRequest_BadRequest (req, sender);
        return true;
    }

    bool const download = req->hasParameter ("download");

    logD_ (_func,
           "channel: ", channel_name, ", "
           "start: ", start_unixtime_sec, ", "
           "duration: ", duration_sec);

    Ref<GetFileSession_Data> const data = grabNew <GetFileSession_Data> ();
    data->moment_nvr_module = this;
    data->weak_sender = sender;
    data->unblock_input_cb = conn_info->getUnblockInputCb();
    data->keepalive = req->getKeepalive();

    Ref<GetFileSession> const get_file_session = grabNewObject <GetFileSession> ();
    {
        StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir->mem());
        get_file_session->init (moment,
                                req,
                                sender,
                                page_pool,
                                vfs,
                                channel_name,
                                start_unixtime_sec,
                                duration_sec,
                                download /* octet_stream_mime */,
                                CbDesc<GetFileSession::Frontend> (&get_file_session_frontend, data, this, data));
    }
    mutex.lock ();
    data->list_el = get_file_sessions.append (get_file_session);
    mutex.unlock ();

    get_file_session->start ();
    conn_info->blockInput ();

    return false;
}

HttpService::HttpHandler const MomentNvrModule::admin_http_handler =
{
    adminHttpRequest,
    NULL /* httpMessageBody */
};

Result
MomentNvrModule::adminHttpRequest (HttpRequest  * const mt_nonnull req,
                                   HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
                                   IpAddress      const /* local_addr */,
                                   Sender       * const mt_nonnull sender,
                                   Memory         const /* msg_body */,
                                   void        ** const mt_nonnull /* ret_msg_data */,
                                   void         * const _self)
{
    MomentNvrModule * const self = static_cast <MomentNvrModule*> (_self);

    logD_ (_func, req->getRequestLine());

    if (req->getNumPathElems() >= 2
        && (equal (req->getPath (1), "rec_on") ||
            equal (req->getPath (1), "rec_off")))
    {
        self->doAdminHttpRequest_rec_on_off (req, sender);
    } else
    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "add_channel"))
    {
        self->doAdminHttpRequest_add_channel (req, sender);
    } else
    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "remove_channel"))
    {
        self->doAdminHttpRequest_remove_channel (req, sender);
    } else {
        logE_ (_func, "Unknown request: ", req->getFullPath());
        self->finishHttpRequest_NotFound (req, sender);
    }

    if (!req->getKeepalive())
        sender->closeAfterFlush ();

    return Result::Success;
}

void
MomentNvrModule::doAdminHttpRequest_rec_on_off (HttpRequest * const mt_nonnull req,
                                                Sender      * const mt_nonnull sender)
{
    ConstMemory const channel_name = req->getParameter ("stream");
    ConstMemory const seq          = req->getParameter ("seq");

    ChannelRecorder::ChannelState channel_state;

    bool const set_on = equal (req->getPath (1), "rec_on");
    ChannelRecorder::ChannelResult res = channel_recorder->setRecording (channel_name, set_on);
    if (res == ChannelRecorder::ChannelResult_Success)
        res = channel_recorder->getChannelState (channel_name, &channel_state);

    if (res == ChannelRecorder::ChannelResult_ChannelNotFound) {
        finishHttpRequest_NotFound (req, sender);
        return;
    } else
    if (res == ChannelRecorder::ChannelResult_Failure) {
        finishHttpRequest_InternalServerError (req, sender);
        return;
    }
    assert (res == ChannelRecorder::ChannelResult_Success);

    MOMENT_SERVER__HEADERS_DATE

    StRef<String> const reply_body = channelStateToJson (&channel_state, seq);
    sender->send (page_pool,
                  true /* do_flush */,
                  MOMENT_SERVER__OK_HEADERS ("text/html", reply_body->len()),
                  "\r\n",
                  reply_body->mem());

    logA_ ("mod_nvr_admin OK ", req->getClientAddress(), " ", req->getRequestLine());
}

void
MomentNvrModule::doAdminHttpRequest_add_channel (HttpRequest * const mt_nonnull req,
                                                 Sender      * const mt_nonnull sender)
{
    ConstMemory const channel_name  = req->getParameter ("conf_file");
    ConstMemory const channel_title = req->getParameter ("title");
    ConstMemory const uri           = req->getParameter ("uri");

    if (   !channel_name.len()
        || !uri.len())
    {
        finishHttpRequest_BadRequest (req, sender);
        return;
    }

    {
        StRef<String> const config_str =
                makeString ("uri = \"", uri, "\";\n",
                            "title = \"", (channel_title.len() ? channel_title : channel_name), "\";\n");

        if (!moment->getChannelManager()->saveChannel (channel_name, config_str)) {
            logE_ (_func, "saveChannel(\"", channel_name, "\") failed");
            finishHttpRequest_InternalServerError (req, sender);
            return;
        }
    }

    finishHttpRequest_Ok (req, sender);
}

void
MomentNvrModule::doAdminHttpRequest_remove_channel (HttpRequest * const mt_nonnull req,
                                                    Sender      * const mt_nonnull sender)
{
    ConstMemory const channel_name = req->getParameter ("conf_file");

    if (!channel_name.len()) {
        finishHttpRequest_BadRequest (req, sender);
        return;
    }

    if (!moment->getChannelManager()->removeChannel (channel_name)) {
        logE_ (_func, "removeChannel(\"", channel_name, "\") failed");
        finishHttpRequest_InternalServerError (req, sender);
        return;
    }

    finishHttpRequest_Ok (req, sender);
}

void
MomentNvrModule::finishHttpRequest_Ok (HttpRequest * const mt_nonnull req,
                                       Sender      * const mt_nonnull sender)
{
    doFinishHttpRequest (req, sender, HttpStatusCode::Ok, "200 OK");
}

void
MomentNvrModule::finishHttpRequest_BadRequest (HttpRequest * const mt_nonnull req,
                                               Sender      * const mt_nonnull sender)
{
    doFinishHttpRequest (req, sender, HttpStatusCode::BadRequest, "400 Bad Request");
}

void
MomentNvrModule::finishHttpRequest_NotFound (HttpRequest * const mt_nonnull req,
                                             Sender      * const mt_nonnull sender)
{
    doFinishHttpRequest (req, sender, HttpStatusCode::NotFound, "404 Not Found");
}

void
MomentNvrModule::finishHttpRequest_InternalServerError (HttpRequest * const mt_nonnull req,
                                                        Sender      * const mt_nonnull sender)
{
    doFinishHttpRequest (req, sender, HttpStatusCode::InternalServerError, "500 Internal Server Error");
}

void
MomentNvrModule::doFinishHttpRequest (HttpRequest    * const mt_nonnull req,
                                      Sender         * const mt_nonnull sender,
                                      HttpStatusCode   const status_code,
                                      ConstMemory      const status_str)
{
    MOMENT_SERVER__HEADERS_DATE

    ConstMemory const reply_body = " (mod_nvr)";
    sender->send (page_pool,
                  true /* do_flush */,
                  MOMENT_SERVER__HEADERS (status_str, "text/plain", status_str.len() + reply_body.len()),
                  "\r\n",
                  status_str, reply_body);

    if (status_code == HttpStatusCode::Ok)
        logA_ ("mod_nvr OK ", req->getClientAddress(), " ", req->getRequestLine());
    else
        logA_ ("mod_nvr ", (Uint32) status_code.val(), " ", req->getClientAddress(), " ", req->getRequestLine());
}

mt_const Result
MomentNvrModule::init (MomentServer * const mt_nonnull moment)
{
    this->moment = moment;

    Ref<MConfig::Config> const config = moment->getConfigManager()->getConfig();

    ConstMemory record_dir_mem;
    {
        ConstMemory const opt_name = "mod_nvr/record_dir";
        bool record_dir_is_set = false;
        record_dir_mem = config->getString (opt_name, &record_dir_is_set);
        if (!record_dir_is_set) {
            logE_ (_func, opt_name, " config option is not set, disabling mod_nvr");
            return Result::Failure;
        }
        logI_ (_func, opt_name, ": ", record_dir_mem);
    }
    record_dir = newString (record_dir_mem);

    Int64 offset_hours = 0;
    if (!configGetInt64_ (config, "mod_nvr/offset_hours", &offset_hours, _func))
//#warning TODO use configGet*() for other options, too
        return Result::Failure;

    Uint64 file_duration_sec = 3600;
    {
        ConstMemory const opt_name = "mod_nvr/file_duration";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &file_duration_sec, file_duration_sec);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", file_duration_sec);
    }

    Uint64 max_age_minutes = 120;
    {
        ConstMemory const opt_name = "mod_nvr/max_age";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &max_age_minutes, max_age_minutes);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", max_age_minutes);
    }
    Uint64 max_age_sec = max_age_minutes * 60;

    Uint64 clean_interval_sec = 5;
    {
        ConstMemory const opt_name = "mod_nvr/clean_interval";
        MConfig::GetResult const res =
                config->getUint64_default (opt_name, &clean_interval_sec, clean_interval_sec);
        if (!res)
            logE_ (_func, "Invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        else
            logI_ (_func, opt_name, ": ", clean_interval_sec);
    }

    page_pool = moment->getPagePool();

    Ref<NamingScheme> const naming_scheme =
            grabNew <DefaultNamingScheme> (file_duration_sec, offset_hours);
    StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (record_dir_mem);

    MOMENT_NVR__INIT

    nvr_vod_service = grabNewObject <NvrVodService> ();
    nvr_vod_service->init (moment, vfs, record_dir_mem);

    channel_recorder = grabNewObject <ChannelRecorder> ();
    channel_recorder->init (moment, vfs, naming_scheme, max_age_sec, clean_interval_sec);

    moment->getHttpManager()->getHttpService()->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&http_handler, this, this),
            "mod_nvr",
            true    /* preassembly */,
            1 << 20 /* 1 Mb */ /* preassembly_limit */,
            true    /* parse_body_params */);

    moment->getHttpManager()->getAdminHttpService()->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&admin_http_handler, this, this),
            "mod_nvr_admin",
            true    /* preassembly */,
            1 << 20 /* 1 Mb */ /* preassembly_limit */,
            true    /* parse_body_params */);

    return Result::Success;
}

MomentNvrModule::MomentNvrModule (Object * const embed_container)
    : Object (embed_container)
{
}

}

