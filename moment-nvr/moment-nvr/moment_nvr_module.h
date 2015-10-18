/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__MOMENT_NVR_MODULE__H__
#define MOMENT_NVR__MOMENT_NVR_MODULE__H__


#include <moment/libmoment.h>

#include <moment-nvr/channel_recorder.h>
#include <moment-nvr/get_file_session.h>
#include <moment-nvr/nvr_vod_service.h>


namespace MomentNvr {

class MomentNvrModule : public Object
{
  private:
    StateMutex mutex;

    mt_const Ref<MomentServer>    moment;
    mt_const Ref<ChannelRecorder> channel_recorder;
    mt_const Ref<PagePool>        page_pool;

    mt_const StRef<String> record_dir;

    mt_const Ref<NvrVodService> nvr_vod_service;

    typedef List< Ref<GetFileSession> > GetFileSessionList;
    mt_mutex (mutex) GetFileSessionList get_file_sessions;

    static StRef<String> channelStateToJson (ChannelRecorder::ChannelState * mt_nonnull channel_state,
                                             ConstMemory seq);

#if 0
// Unused
    struct Frontend {
        void (*done) (Result  res,
                      void   *cb_data);
    };
#endif

    mt_iface (GetFileSession::Frontend)
      static GetFileSession::Frontend const get_file_session_frontend;

      struct GetFileSession_Data;

      static void getFileSession_done (Result  res,
                                       void   *_data);
    mt_iface_end

    mt_iface (HttpService::HttpHandler)
      static HttpService::HttpHandler const http_handler;

      static Result httpRequest (HttpRequest  * mt_nonnull req,
                                 HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                                 IpAddress     local_addr,
                                 Sender       * mt_nonnull sender,
                                 Memory        msg_body,
                                 void        ** mt_nonnull ret_msg_data,
                                 void         *_self);
    mt_iface_end

    void doHttpRequest_unixtime (HttpRequest * mt_nonnull req,
                                 Sender      * mt_nonnull sender);

    void doHttpRequest_channel_state (HttpRequest * mt_nonnull req,
                                      Sender      * mt_nonnull sender);

    bool doHttpRequest_file (HttpRequest                     * mt_nonnull req,
                             HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                             Sender                          * mt_nonnull sender);

    mt_iface (HttpService::HttpHandler)
      static HttpService::HttpHandler const admin_http_handler;

      static Result adminHttpRequest (HttpRequest  * mt_nonnull req,
                                      HttpService::HttpConnectionInfo * mt_nonnull conn_info,
                                      IpAddress     local_addr,
                                      Sender       * mt_nonnull sender,
                                      Memory        msg_body,
                                      void        ** mt_nonnull ret_msg_data,
                                      void         *_self);
    mt_iface_end

    void doAdminHttpRequest_rec_on_off (HttpRequest * mt_nonnull req,
                                        Sender      * mt_nonnull sender);

    void doAdminHttpRequest_add_channel (HttpRequest * mt_nonnull req,
                                         Sender      * mt_nonnull sender);

    void doAdminHttpRequest_remove_channel (HttpRequest * mt_nonnull req,
                                            Sender      * mt_nonnull sender);

    void finishHttpRequest_Ok (HttpRequest * mt_nonnull req,
                               Sender      * mt_nonnull sender);

    void finishHttpRequest_BadRequest (HttpRequest * mt_nonnull req,
                                       Sender      * mt_nonnull sender);

    void finishHttpRequest_NotFound (HttpRequest * mt_nonnull req,
                                     Sender      * mt_nonnull sender);

    void finishHttpRequest_InternalServerError (HttpRequest * mt_nonnull req,
                                                Sender      * mt_nonnull sender);

    void doFinishHttpRequest (HttpRequest    * mt_nonnull req,
                              Sender         * mt_nonnull sender,
                              HttpStatusCode  status_code,
                              ConstMemory     status_str);

  public:
    mt_const Result init (MomentServer * mt_nonnull moment);

    MomentNvrModule (Object *embed_container);
};

}


#endif /* MOMENT_NVR__MOMENT_NVR_MODULE__H__ */

