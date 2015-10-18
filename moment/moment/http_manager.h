/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__HTTP_MANAGER__H__
#define MOMENT__HTTP_MANAGER__H__


#include <libmary/libmary.h>
#include <mconfig/mconfig.h>
#include <moment/util_moment_http.h>


namespace Moment {

using namespace M;

class HttpManager : public Object
{
  public:
    struct HttpRequestResult
    {
        enum Value {
            Success,
            NotFound
        };
        operator Value () const { return value; }
        HttpRequestResult (Value const value) : value (value) {}
        HttpRequestResult () {}
      private:
        Value value;
    };

    struct HttpRequestHandler
    {
        HttpRequestResult (*httpRequest) (HttpRequest * mt_nonnull req,
//#error TODO conn_info
                                          IpAddress    local_addr,
                                          Sender      * mt_nonnull conn_sender,
                                          Memory       msg_body,
                                          void        *cb_data);
    };

  private:
    class HttpHandlerEntryList_name;
    struct HttpHandlerEntry : public IntrusiveListElement< HttpHandlerEntryList_name >
    {
        Cb<HttpRequestHandler> cb;
    };
    typedef IntrusiveList< HttpHandlerEntry,
                           HttpHandlerEntryList_name,
                           DeleteAction<HttpHandlerEntry> >
            HttpHandlerEntryList;

    mt_const Ref<PagePool> page_pool;

    mt_const HttpHandlerEntryList  admin_http_handlers;
    mt_const HttpHandlerEntryList server_http_handlers;

    Embed<HttpService> http_service;

    Embed<HttpService> separate_admin_http_service;
    mt_const HttpService *admin_http_service_ptr;

    mt_iface (HttpService::HttpHandler)
      static HttpService::HttpHandler const admin_http_handler;

      static Result adminHttpRequest (HttpRequest  * mt_nonnull req,
                                      HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                                      IpAddress     local_addr,
                                      Sender       * mt_nonnull conn_sender,
                                      Memory        msg_body,
                                      void        ** mt_nonnull ret_msg_data,
                                      void         *_self);
    mt_iface_end

    mt_iface (HttpService::HttpHandler)
      static HttpService::HttpHandler const server_http_handler;

      static Result serverHttpRequest (HttpRequest  * mt_nonnull req,
                                       HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                                       IpAddress     local_addr,
                                       Sender       * mt_nonnull conn_sender,
                                       Memory        msg_body,
                                       void        ** mt_nonnull ret_msg_data,
                                       void         *_self);
    mt_iface_end

  public:
    mt_const void addAdminRequestHandler  (CbDesc<HttpRequestHandler> const &cb);
    mt_const void addServerRequestHandler (CbDesc<HttpRequestHandler> const &cb);

    HttpService* getHttpService      () { return http_service; }
    HttpService* getAdminHttpService () { return admin_http_service_ptr; }

    mt_const Result init (MConfig::Config * mt_nonnull config,
                          ServerContext   * mt_nonnull server_ctx,
                          PagePool        * mt_nonnull page_pool);

    HttpManager (EmbedContainer *embed_container);
};

}


#endif /* MOMENT__HTTP_MANAGER__H__ */

