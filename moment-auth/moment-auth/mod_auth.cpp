/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>
#include <moment/libmoment.h>
#include <moment/moment_instance.h>

#include <moment-auth/util_parse.h>


using namespace Moment;
using namespace MomentAuth;


namespace {
class MomentAuthModule : public Object
{
  private:
    mt_const Uint32 mode;

    mt_const StRef<String> this_host;

    mt_const StRef<String> watch_req;
    mt_const bool watch_req_enabled;
    mt_const bool watch_req_has_params;

    mt_const StRef<String> watch_restream_req;
    mt_const bool watch_restream_req_enabled;
    mt_const bool watch_restream_req_has_params;

    mt_const StRef<String> stream_req;
    mt_const bool stream_req_enabled;
    mt_const bool stream_req_has_params;

    mt_const StRef<String> disconnected_req;
    mt_const bool disconnected_req_enabled;
    mt_const bool disconnected_req_has_params;

    Embed<HttpClient> http_client;

    void sendDisconnected (ConstMemory auth_key,
                           IpAddress   client_addr,
                           ConstMemory stream_name);

    mt_iface (AuthManager::AuthBackend)
      static AuthManager::AuthBackend const auth_backend;

      struct CheckAuthorization_Data : public Referenced
      {
          Uint32 mode;
          Cb<AuthManager::CheckAuthorizationCallback> cb;
      };

      static bool checkAuthorization (AuthManager::AuthAction   auth_action,
                                      ConstMemory               stream_name,
                                      ConstMemory               auth_key,
                                      IpAddress                 client_addr,
                                      CbDesc<AuthManager::CheckAuthorizationCallback> const &cb,
                                      bool                     * mt_nonnull ret_authorized,
                                      StRef<String>            * mt_nonnull ret_stream_name,
                                      StRef<String>            * mt_nonnull ret_restream_reply,
                                      void                     *_self);

      static void clientDisconnected (ConstMemory               auth_key,
                                      IpAddress                 client_addr,
                                      ConstMemory               stream_name,
                                      void                     *_self);
    mt_iface_end

    mt_iface (HttpClient::HttpResponseHandler)
      static HttpClient::HttpResponseHandler const auth_response_handler;

      static Result authHttpResponse (HttpRequest  *resp,
                                      Memory        msg_body,
                                      void        ** mt_nonnull /* ret_msg_data */,
                                      void         *_data);
    mt_iface_end

  public:
    mt_const void init (MomentServer * mt_nonnull moment,
                        Uint32        mode,
                        IpAddress     auth_addr,
                        ConstMemory   auth_host,
                        ConstMemory   this_host,
                        ConstMemory   watch_req,
                        bool          watch_req_enabled,
                        ConstMemory   watch_restream_req,
                        bool          watch_restream_req_enabled,
                        ConstMemory   stream_req,
                        bool          stream_req_enabled,
                        ConstMemory   disconnected_req,
                        bool          disconnected_req_enabled);

    MomentAuthModule (EmbedContainer *embed_container);
};
}

AuthManager::AuthBackend const MomentAuthModule::auth_backend = {
    checkAuthorization,
    clientDisconnected
};

bool
MomentAuthModule::checkAuthorization (AuthManager::AuthAction   const auth_action,
                                      ConstMemory               const stream_name,
                                      ConstMemory               const auth_key,
                                      IpAddress                 const client_addr,
                                      CbDesc<AuthManager::CheckAuthorizationCallback> const &cb,
                                      bool                    * const mt_nonnull ret_authorized,
                                      StRef<String>           * const mt_nonnull ret_stream_name,
                                      StRef<String>           * const mt_nonnull ret_restream_reply,
                                      void                    * const _self)
{
    MomentAuthModule * const self = static_cast <MomentAuthModule*> (_self);

    *ret_authorized = false;
    *ret_stream_name = NULL;
    *ret_restream_reply = NULL;

    logD_ (_self_func, "stream_name: ", stream_name, ", "
           "auth_key: ", auth_key, ", "
           "client_addr: ", client_addr);

    Ref<CheckAuthorization_Data> const data = grabNew <CheckAuthorization_Data> ();
    data->mode = self->mode;
    data->cb = cb;

    switch (self->mode) {
        default:
        case 0: {
            StRef<String> req_str;
            switch (auth_action.val()) {
                case AuthManager::AuthAction::Watch: {
                    if (self->watch_req_enabled) {
                        req_str = makeString ("/", self->watch_req->mem(),
                                              (self->watch_req_has_params ? "&" : "?"));
                    }
                } break;
                case AuthManager::AuthAction::WatchRestream: {
                    if (self->watch_restream_req_enabled) {
                        req_str = makeString ("/", self->watch_restream_req->mem(),
                                              (self->watch_restream_req_has_params ? "&" : "?"));
                    }
                } break;
                case AuthManager::AuthAction::Stream: {
                    if (self->stream_req_enabled) {
                        req_str = makeString ("/", self->stream_req->mem(),
                                              (self->stream_req_has_params ? "&" : "?"));
                    }
                } break;
                default:
                    unreachable ();
            }
            if (!req_str) {
                *ret_authorized = true;
                return true;
            }

            req_str = makeString (req_str->mem(),
                                  "host=",    self->this_host->mem(),
                                  "&client=", IpAddress_NoPort (client_addr),
                                  "&stream=", stream_name,
                                  "&auth=",   auth_key);
            logD_ (_self_func, "req_str: ", req_str);

            if (!self->http_client->httpGet (
                        req_str->mem(),
                        CbDesc<HttpClient::HttpResponseHandler> (&auth_response_handler, data, NULL, data),
                        true  /* preassembly */,
                        false /* parse_body_params */,
                        true  /* use_http_1_0 */))
            {
                logE_ (_self_func, "httpGet() failed");
                *ret_authorized = false;
                return true;
            }
        } break;
        case 1: {
            StRef<String> req_str;
            switch (auth_action.val()) {
                case AuthManager::AuthAction::Watch: {
                    if (self->watch_req_enabled)
                        req_str = makeString ("/", self->watch_req->mem());
                } break;
                case AuthManager::AuthAction::WatchRestream: {
                    if (self->watch_restream_req_enabled)
                        req_str = makeString ("/", self->watch_restream_req->mem());
                } break;
                case AuthManager::AuthAction::Stream: {
                    if (self->stream_req_enabled)
                        req_str = makeString ("/", self->stream_req->mem());
                } break;
                default:
                    unreachable ();
            }
            if (!req_str) {
                *ret_authorized = true;
                return true;
            }

            if (!self->http_client->httpPost (
                        req_str->mem(),
                        // makeString ("data={\"id\":\"", auth_key, "\",\"clientAddr\":\"", IpAddress_NoPort (client_addr), "\"}"),
                        makeString ("data=%7B%22id%22%3A%22", auth_key, "%22%2C%22clientAddr%22%3A%22", IpAddress_NoPort (client_addr), "%22%7D"),
                        CbDesc<HttpClient::HttpResponseHandler> (&auth_response_handler, data, NULL, data),
                        true /* preassembly */))
            {
                logE_ (_self_func, "httpPost() failed");
                *ret_authorized = false;
                return true;
            }

            logD_ (_self_func, "httpPost() returned");
        } break;
    }

    return false;
}

HttpClient::HttpResponseHandler const MomentAuthModule::auth_response_handler = {
    authHttpResponse,
    NULL /* httpResponseBody */
};

Result
MomentAuthModule::authHttpResponse (HttpRequest   * const resp,
                                    Memory          const msg_body,
                                    void         ** const mt_nonnull /* ret_msg_data */,
                                    void          * const _data)
{
    CheckAuthorization_Data * const data = static_cast <CheckAuthorization_Data*> (_data);
    // 'self' is not available: Cb weak_obj is NULL.

    if (!resp) {
        logE_ (_func, "request error");
        data->cb.call_ (false /* authorized */, ConstMemory() /* stream_name */, ConstMemory() /* restream_reply */);
        return Result::Success;
    }

    if (logLevelOn_ (LogLevel::Debug)) {
        logLock ();
        log_locked__ (LogLevel::Debug, _func, "length: ", msg_body.len());
        logHex_locked__ (LogLevel::Debug, msg_body, _func);
        logUnlock ();
    }

    bool authorized = false;
    ConstMemory stream_name;
    ConstMemory restream_reply;
    switch (data->mode) {
        default:
        case 0: {
            Byte *buf = msg_body.buf();
            Size  len = msg_body.len();

            skipWhitespace (&buf, &len);

            ConstMemory const ok_mem = "OK";
            authorized = (len >= ok_mem.len() && equal (ok_mem, ConstMemory (buf, len)));

            if (authorized) {
                buf += ok_mem.len();
                len -= ok_mem.len();

                skipWhitespace (&buf, &len);

                restream_reply = ConstMemory (buf, len);
                logD_ (_func, "restream_reply: ", restream_reply);
            }
        } break;
        case 1: {
            // { "result":"success", "id"="blabla" }

            Byte *buf = msg_body.buf();
            Size  len = msg_body.len();

            ConstMemory result;
            ConstMemory id;

            while (len) {
                skipToAlphanumeric (&buf, &len);

                Byte * const name_beg = buf;
                skipToNonAlphanumeric (&buf, &len);
                Byte * const name_end = buf;
                Memory name (name_beg, name_end - name_beg);
                toLowercase (name);

                skipToAfterSymbol (&buf, &len, ':');
                skipToAlphanumeric (&buf, &len);

                Byte * const value_beg = buf;
                skipToNonAlphanumeric (&buf, &len);
                Byte * const value_end = buf;
                Memory value (value_beg, value_end - value_beg);

                if (equal (name, "result")) {
                    toLowercase (value);
                    result = value;
                } else
                if (equal (name, "id")) {
                    id = value;
                }

                skipToAfterSymbol (&buf, &len, ',');
            }

            if (equal (result, "success")) {
                authorized = true;
                stream_name = id;
            }
        } break;
    }

    data->cb.call_ (authorized, stream_name, restream_reply);

    return Result::Success;
}

void
MomentAuthModule::clientDisconnected (ConstMemory   const auth_key,
                                      IpAddress     const client_addr,
                                      ConstMemory   const stream_name,
                                      void        * const _self)
{
    MomentAuthModule * const self = static_cast <MomentAuthModule*> (_self);
    self->sendDisconnected (auth_key, client_addr, stream_name);
}

static Result disconnectedHttpResponse (HttpRequest  *resp,
                                        Memory        msg_body,
                                        void        ** mt_nonnull /* ret_msg_data */,
                                        void         *cb_data);

static HttpClient::HttpResponseHandler const disconnected_response_handler = {
    disconnectedHttpResponse,
    NULL /* httpResponseBody */
};

void
MomentAuthModule::sendDisconnected (ConstMemory const auth_key,
                                    IpAddress   const client_addr,
                                    ConstMemory const stream_name)
{
    if (!disconnected_req_enabled)
        return;

    switch (mode) {
        default:
        case 0: {
            StRef<String> const req_str =
                    makeString ("/", disconnected_req->mem(), (disconnected_req_has_params ? "&" : "?"),
                                "host=",    this_host->mem(),
                                "&client=", IpAddress_NoPort (client_addr),
                                "&stream=", stream_name,
                                "&auth=",   auth_key);
            logD_ (_this_func, "req_str: ", req_str);

            if (!http_client->httpGet (req_str->mem(),
                                       CbDesc<HttpClient::HttpResponseHandler> (&disconnected_response_handler,
                                                                                NULL,
                                                                                NULL),
                                       true  /* preassembly */,
                                       false /* parse_body_params */,
                                       true  /* use_http_1_0 */ ))
            {
                logE_ (_this_func, "httpGet() failed");
            }
        } break;
    }
}

static Result disconnectedHttpResponse (HttpRequest  * const resp,
                                        Memory         const /* msg_body */,
                                        void        ** const mt_nonnull /* ret_msg_data */,
                                        void         * const /* cb_data */)
{
    if (!resp)
        logE_ (_func, "request error");

    return Result::Success;
}

mt_const void
MomentAuthModule::init (MomentServer * const mt_nonnull moment,
                        Uint32         const mode,
                        IpAddress      const auth_addr,
                        ConstMemory    const auth_host,
                        ConstMemory    const this_host,
                        ConstMemory    const watch_req,
                        bool           const watch_req_enabled,
                        ConstMemory    const watch_restream_req,
                        bool           const watch_restream_req_enabled,
                        ConstMemory    const stream_req,
                        bool           const stream_req_enabled,
                        ConstMemory    const disconnected_req,
                        bool           const disconnected_req_enabled)
{
    this->mode = mode;

    this->this_host = newString (this_host);

    this->watch_req = newString (watch_req);
    this->watch_req_enabled = watch_req_enabled;
    watch_req_has_params = (bool) strchr (this->watch_req->cstr(), '?');

    this->watch_restream_req = newString (watch_restream_req);
    this->watch_restream_req_enabled = watch_restream_req_enabled;
    watch_restream_req_has_params = (bool) strchr (this->watch_restream_req->cstr(), '?');

    this->stream_req = newString (stream_req);
    this->stream_req_enabled = stream_req_enabled;
    stream_req_has_params = (bool) strchr (this->stream_req->cstr(), '?');

    this->disconnected_req = newString (disconnected_req);
    this->disconnected_req_enabled = disconnected_req_enabled;
    disconnected_req_has_params = (bool) strchr (this->disconnected_req->cstr(), '?');

    {
        http_client->init (moment->getServerApp()->getServerContext()->selectThreadContext(),
                           moment->getPagePool(),
                           auth_addr,
                           auth_host,
                           false /* keepalive */,
                           1 << 20 /* 1 Mb */ /* preassembly_limit */);
    }

    moment->getAuthManager()->setAuthBackend (CbDesc<AuthManager::AuthBackend> (&auth_backend, this, this));
}

MomentAuthModule::MomentAuthModule (EmbedContainer * const embed_container)
    : Object      (embed_container),
      mode        (0),
      http_client (this /* embed_container */)
{
}

namespace MomentAuth {

Result momentAuthInit (MomentInstance * const mt_nonnull moment_instance)
{
    logD_ (_func_);

    Ref<MomentServer> const moment_server = moment_instance->getMomentServer();
    Ref<MConfig::Config> const config = moment_server->getConfigManager()->getConfig();

    {
        bool enable = true;
        if (!configGetBoolean (config, "mod_auth/enable", &enable, enable, _func))
            return Result::Failure;

        if (!enable) {
            logI_ (_func, "mod_auth module is not enabled");
            return Result::Success;
        }
    }

    Uint64 mode = 0;
    if (!configGetUint64 (config, "mod_auth/mode", &mode, mode, _func))
        return Result::Failure;

    ConstMemory auth_host;
    IpAddress auth_addr;
    {
        ConstMemory const opt_name = "mod_auth/auth_host";
        auth_host = config->getString (opt_name);
        if (auth_host.isNull()) {
            logI_ (_func, opt_name,  " is empty, disabling mod_auth");
            return Result::Success;
        }

        logD_ (_func, opt_name, ": ", auth_host);

        if (!setIpAddress_default (auth_host,
                                   ConstMemory() /* default_host */,
                                   80            /* default_port */,
                                   false         /* allow_any_host */,
                                   &auth_addr))
        {
            logE_ (_func, "bad ", opt_name, ": ", auth_host, ", disabling mod_auth");
            return Result::Failure;
        }
    }

    ConstMemory this_host;
    {
        ConstMemory const opt_name = "mod_auth/this_host";
        this_host = config->getString (opt_name);
        logD_ (_func, opt_name, ": ", this_host);
    }

    ConstMemory watch_req;
    bool watch_req_enabled = false;
    {
        ConstMemory const opt_name = "mod_auth/watch_req";
        watch_req = config->getString (opt_name, &watch_req_enabled);
        logD_ (_func, opt_name, ": ", watch_req);

        if (!watch_req_enabled)
            logD_ (_func, "watch auth check is not enabled");
    }

    ConstMemory watch_restream_req;
    bool watch_restream_req_enabled = false;
    {
        ConstMemory const opt_name = "mod_auth/watch_restream_req";
        watch_restream_req = config->getString (opt_name, &watch_restream_req_enabled);
        logD_ (_func, opt_name, ": ", watch_restream_req);

        if (!watch_restream_req_enabled)
            logD_ (_func, "watch_restream auth check is not enabled");
    }

    ConstMemory stream_req;
    bool stream_req_enabled = false;
    {
        ConstMemory const opt_name = "mod_auth/stream_req";
        stream_req = config->getString (opt_name, &stream_req_enabled);
        logD_ (_func, opt_name, ": ", stream_req);

        if (!stream_req_enabled)
            logD_ (_func, "stream auth check is not enabled");
    }

    ConstMemory disconnected_req;
    bool disconnected_req_enabled = false;
    {
        ConstMemory const opt_name = "mod_auth/disconnected_req";
        disconnected_req = config->getString (opt_name, &disconnected_req_enabled);
        logD_ (_func, opt_name, ": ", disconnected_req);

        if (!disconnected_req_enabled)
            logD_ (_func, "disconnect auth notification is not enabled");
    }

    Ref<MomentAuthModule> const moment_auth_module = grabNewObject <MomentAuthModule> ();
    moment_auth_module->init (moment_server,
                              (Uint32) mode,
                              auth_addr,
                              auth_host,
                              this_host,
                              watch_req,
                              watch_req_enabled,
                              watch_restream_req,
                              watch_restream_req_enabled,
                              stream_req,
                              stream_req_enabled,
                              disconnected_req,
                              disconnected_req_enabled);

    moment_instance->addDeletionData (moment_auth_module);
    return Result::Success;
}

}

#ifndef MOMENT_STATIC_MODULES
  extern "C" bool
  libMary_moduleInit (void * const moment_instance_)
          { return MomentAuth::momentAuthInit (static_cast <MomentInstance*> (moment_instance_)); }
#endif

