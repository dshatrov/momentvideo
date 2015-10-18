#include <moment/util_config.h>

#include <moment/http_manager.h>


namespace Moment {

HttpService::HttpHandler const HttpManager::admin_http_handler = {
    adminHttpRequest,
    NULL /* httpMessageBody */
};

Result
HttpManager::adminHttpRequest (HttpRequest  * const mt_nonnull req,
                               HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
                               IpAddress      const local_addr,
                               Sender       * const mt_nonnull conn_sender,
                               Memory        const msg_body,
                               void        ** const mt_nonnull /* ret_msg_data */,
                               void         * const _self)
{
    HttpManager * const self = static_cast <HttpManager*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "stat"))
    {
        PagePool::PageListInfo page_list;

        self->page_pool->printToPages (
                &page_list,
                "<html>"
                "<body>"
                "<p>Stats</p>"
                "<table>");
        {
            List<Stat::StatParam> stat_params;
            getStat()->getAllParams (&stat_params);

            List<Stat::StatParam>::iter iter (stat_params);
            while (!stat_params.iter_done (iter)) {
                Stat::StatParam * const stat_param = &stat_params.iter_next (iter)->data;

                self->page_pool->printToPages (
                        &page_list,
                        "<tr>"
                            "<td>", stat_param->param_name, "</td>");

                if (stat_param->param_type == Stat::ParamType_Int64) {
                    self->page_pool->printToPages (
                            &page_list,
                            "<td>", stat_param->int64_value, "</td>");
                } else {
                    assert (stat_param->param_type == Stat::ParamType_Double);
                    self->page_pool->printToPages (
                            &page_list,
                            "<td>", stat_param->double_value, "</td>");
                }

                self->page_pool->printToPages (
                        &page_list,
                            "<td>", stat_param->param_desc, "</td>"
                        "</tr>");
            }
        }
        self->page_pool->printToPages (
                &page_list,
                "</table>"
                "</body>"
                "</html>");

        conn_sender->send (
                self->page_pool,
                false /* do_flush */,
                MOMENT_SERVER__OK_HEADERS ("text/html", page_list.data_len),
                "\r\n");
        conn_sender->sendPages (self->page_pool, page_list.first, /*msg_offs=*/ 0, page_list.data_len, true /* do_flush */);

        logA_ ("file 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else {
        HttpHandlerEntryList::iterator iter (self->admin_http_handlers);
        while (!iter.done()) {
            HttpHandlerEntry * const handler_entry = iter.next ();

            if (handler_entry->cb && handler_entry->cb->httpRequest) {
                HttpRequestResult res;
                if (!handler_entry->cb.call_ret (&res, handler_entry->cb->httpRequest, req, local_addr, conn_sender, msg_body))
                    continue;

                if (res == HttpRequestResult::Success)
                    return Result::Success;

                assert (res == HttpRequestResult::NotFound);
            }
        }

        logE_ (_func, "Unknown admin HTTP request: ", req->getFullPath());

        ConstMemory const reply_body = "Unknown command";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
                           "\r\n",
                           reply_body);

        logA_ ("moment_server__admin 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}

HttpService::HttpHandler const HttpManager::server_http_handler = {
    serverHttpRequest,
    NULL /* httpMessageBody */
};

Result
HttpManager::serverHttpRequest (HttpRequest  * const mt_nonnull req,
                                HttpService::HttpConnectionInfo * const mt_nonnull /* conn_info */,
                                IpAddress      const local_addr,
                                Sender       * const mt_nonnull conn_sender,
                                Memory         const msg_body,
                                void        ** const mt_nonnull /* ret_msg_data */,
                                void         * const _self)
{
    HttpManager * const self = static_cast <HttpManager*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE;

    {
        HttpHandlerEntryList::iterator iter (self->server_http_handlers);
        while (!iter.done()) {
            HttpHandlerEntry * const handler_entry = iter.next ();

            if (handler_entry->cb && handler_entry->cb->httpRequest) {
                HttpRequestResult res;
                if (!handler_entry->cb.call_ret (&res, handler_entry->cb->httpRequest, req, local_addr, conn_sender, msg_body))
                    continue;

                if (res == HttpRequestResult::Success)
                    return Result::Success;

                assert (res == HttpRequestResult::NotFound);
            }
        }

        logE_ (_func, "Unknown server HTTP request: ", req->getFullPath());

        ConstMemory const reply_body = "Unknown command";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
                           "\r\n",
                           reply_body);

        logA_ ("moment_server__server 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    if (!req->getKeepalive())
        conn_sender->closeAfterFlush();

    return Result::Success;
}

void
HttpManager::addAdminRequestHandler (CbDesc<HttpRequestHandler> const &cb)
{
    HttpHandlerEntry * const handler_entry = new (std::nothrow) HttpHandlerEntry;
    assert (handler_entry);
    handler_entry->cb = cb;
    admin_http_handlers.append (handler_entry);
}

void
HttpManager::addServerRequestHandler (CbDesc<HttpRequestHandler> const &cb)
{
    HttpHandlerEntry * const handler_entry = new (std::nothrow) HttpHandlerEntry;
    assert (handler_entry);
    handler_entry->cb = cb;
    server_http_handlers.append (handler_entry);
}

static char const opt_name__http_keepalive_timeout[]  = "http/keepalive_timeout";
static char const opt_name__http_no_keepalive_conns[] = "http/no_keepalive_conns";
static char const opt_name__http_http_bind[]          = "http/http_bind";
static char const opt_name__http_admin_bind[]         = "http/admin_bind";

static const Uint64 default__http__keepalive_timeout  =  60;

mt_const Result
HttpManager::init (MConfig::Config * const mt_nonnull config,
                   ServerContext   * const mt_nonnull server_ctx,
                   PagePool        * const mt_nonnull page_pool)
{
    this->page_pool = page_pool;

    Uint64 http_keepalive_timeout;
    if (!configGetUint64 (config,
                          opt_name__http_keepalive_timeout,
                          &http_keepalive_timeout,
                          default__http__keepalive_timeout,
                          _func))
    {
        return Result::Failure;
    }

    bool no_keepalive_conns;
    if (!configGetBoolean (config, opt_name__http_no_keepalive_conns, &no_keepalive_conns, false, _func))
        return Result::Failure;

    bool http_bind_valid = false;
    IpAddress http_bind_addr;
    {
        ConstMemory const opt_name = opt_name__http_http_bind; 
        ConstMemory const opt_val = config->getString_default (opt_name, ":8080");
        logI_ (_func, opt_name, ": ", opt_val);
        if (opt_val.len() == 0) {
            logI_ (_func, "HTTP service is not bound to any port "
                   "and won't accept any connections. "
                   "Set \"", opt_name, "\" option to bind the service.");
        } else {
            if (!setIpAddress_default (opt_val,
                                       ConstMemory() /* default_host */,
                                       8080          /* default_port */,
                                       true          /* allow_any_host */,
                                       &http_bind_addr))
            {
                logE_ (_func, "setIpAddress_default() failed (http)");
                return Result::Failure;
            }

            http_bind_valid = true;
        }
    }

    bool http_admin_bind_valid = false;
    IpAddress http_admin_bind_addr;
    {
        ConstMemory const opt_name = opt_name__http_admin_bind; 
        ConstMemory const opt_val = config->getString_default (opt_name, ":8080");
        logI_ (_func, opt_name, ": ", opt_val);
        if (opt_val.len() == 0) {
            logI_ (_func, "HTTP admin service is not bound to any port "
                   "and won't accept any connections. "
                   "Set \"", opt_name, "\" option to bind the service.");
        } else {
            if (!setIpAddress_default (opt_val,
                                       ConstMemory() /* default_host */,
                                       8080          /* default_port */,
                                       true          /* allow_any_host */,
                                       &http_admin_bind_addr))
            {
                logE_ (_func, "setIpAddress_default() failed (http_admin)");
                return Result::Failure;
            }

            http_admin_bind_valid = true;
        }
    }

    if (http_bind_valid) {
        if (!http_service->init (server_ctx->getMainThreadContext()->getPollGroup(),
                                 server_ctx->getMainThreadContext()->getTimers(),
                                 server_ctx->getMainThreadContext()->getDeferredProcessor(),
                                 page_pool,
                                 http_keepalive_timeout * 1000000 /* microseconds */,
                                 no_keepalive_conns))
        {
            logE_ (_func, "http_service.init() failed: ", exc->toString());
            return Result::Failure;
        }

        if (!http_service->bind (http_bind_addr)) {
            logE_ (_func, "http_service.bind() failed (http): ", exc->toString());
            return Result::Failure;
        }

        if (!http_service->start ()) {
            logE_ (_func, "http_service.start() failed (http): ", exc->toString());
            return Result::Failure;
        }
    }

    if (http_admin_bind_valid) {
        if (http_admin_bind_addr == http_bind_addr) {
            admin_http_service_ptr = http_service;
        } else {
            if (!separate_admin_http_service->init (server_ctx->getMainThreadContext()->getPollGroup(),
                                                    server_ctx->getMainThreadContext()->getTimers(),
                                                    server_ctx->getMainThreadContext()->getDeferredProcessor(),
                                                    page_pool,
                                                    http_keepalive_timeout * 1000000 /* microseconds */,
                                                    no_keepalive_conns))
            {
                logE_ (_func, "admin_http_service.init() failed: ", exc->toString());
                return Result::Failure;
            }

            if (!separate_admin_http_service->bind (http_admin_bind_addr)) {
                logE_ (_func, "http_service.bind() failed (admin): ", exc->toString());
                return Result::Failure;
            }

            if (!separate_admin_http_service->start ()) {
                logE_ (_func, "http_service.start() failed (admin): ", exc->toString());
                return Result::Failure;
            }
        }
    }

    admin_http_service_ptr->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&admin_http_handler, this, this),
            "admin");

    http_service->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&server_http_handler, this, this),
            "server");

    return Result::Success;
}

HttpManager::HttpManager (EmbedContainer * const embed_container)
    : Object                      (embed_container),
      http_service                (this /* embed_container */),
      separate_admin_http_service (this /* embed_container */),
      admin_http_service_ptr      (separate_admin_http_service)
{
}

}

