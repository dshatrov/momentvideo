/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>
#include <moment/moment_instance.h>

#include <moment-rtsp/rtsp_fetch_protocol.h>
#include <moment-rtsp/rtsp_service.h>


namespace MomentRtsp {

using namespace Moment;

namespace {
class MomentRtspModule : public Object
{
  private:
    mt_const IpAddress rtsp_bind_addr;
    mt_const bool rtsp_bind_valid;

    Embed<RtspService> rtsp_service;

  public:
    Result start ();

    mt_const Result init (MomentServer * mt_nonnull moment);

    MomentRtspModule (EmbedContainer *embed_container);
};
}

Result
MomentRtspModule::start ()
{
    if (rtsp_bind_valid) {
        if (!rtsp_service->bind (rtsp_bind_addr)) {
            logE_ (_func, "rtsp_service.bind() failed: ", exc->toString());
            return Result::Failure;
        }

        if (!rtsp_service->start ()) {
            logE_ (_func, "rtsp_service.start() failed");
            return Result::Failure;
        }
    }

    return Result::Success;
}

mt_const Result
MomentRtspModule::init (MomentServer * const mt_nonnull moment)
{
    logI_ (_func, "initializing mod_rtsp");

    Ref<MConfig::Config> const config = moment->getConfigManager()->getConfig();

    {
        bool enable = true;
        if (!configGetBoolean (config, "mod_rtsp/enable", &enable, enable, _func))
            return Result::Failure;

        if (!enable) {
            logI_ (_func, "mod_rtsp is not enabled");
            return Result::Success;
        }
    }

    bool enable_tcp  = true;
    bool tcp_only    = false;
    bool udp_silence = false;
    if (   !configGetBoolean_ (config, "mod_rtsp/enable_tcp",  &enable_tcp,  _func)
        || !configGetBoolean_ (config, "mod_rtsp/tcp_only",    &tcp_only,    _func)
        || !configGetBoolean_ (config, "mod_rtsp/udp_silence", &udp_silence, _func))
    {
        return Result::Failure;
    }

    {
        Ref<RtspFetchProtocol> const rtsp_fetch_proto = grabNewObject <RtspFetchProtocol> ();
        if (!rtsp_fetch_proto->init (moment))
            return Result::Failure;

        moment->getSourceManager()->addFetchProtocol ("rtsp", rtsp_fetch_proto);
    }

    {
        rtsp_bind_valid = false;

        ConstMemory const opt_name = "mod_rtsp/rtsp_bind";
        ConstMemory const opt_val = config->getString_default (opt_name, ":5554");
        logI_ (_func, opt_name, ": ", opt_val);
        if (opt_val.len() == 0) {
            logI_ (_func, "RTSP service is not bound to any port "
                   "and won't accept  any connections. "
                   "Set \"", opt_name, "\" option to bind the service.");
        } else {
            if (!setIpAddress_default (opt_val,
                                       ConstMemory() /* default_host */,
                                       5554          /* default_port */,
                                       true          /* allow_any_host */,
                                       &rtsp_bind_addr))
            {
                logE_ (_func, "setIpAddress_default() failed (http)");
            } else {
                rtsp_bind_valid = true;
            }
        }
    }

    Uint64 timeout_check_interval_sec     = 5;
    Uint64 session_request_timeout_sec    = 65;
    Uint64 connection_request_timeout_sec = 90;
    if (   !configGetUint64_ (config, "mod_rtsp/timeout_check_interval",     &timeout_check_interval_sec,     _func)
        || !configGetUint64_ (config, "mod_rtsp/session_request_timeout",    &session_request_timeout_sec,    _func)
        || !configGetUint64_ (config, "mod_rtsp/connection_request_timeout", &connection_request_timeout_sec, _func))
    {
        return Result::Failure;
    }

    if (rtsp_bind_valid) {
        if (!rtsp_service->init (moment,
                                 moment->getPagePool(),
                                 enable_tcp,
                                 tcp_only,
                                 udp_silence,
                                 timeout_check_interval_sec * 1000,
                                 session_request_timeout_sec * 1000,
                                 connection_request_timeout_sec * 1000))
        {
            logE_ (_func, "rtsp_service.init() failed");
            return Result::Failure;
        }
    }

    return Result::Success;
}

MomentRtspModule::MomentRtspModule (EmbedContainer * const embed_container)
    : Object          (embed_container),
      rtsp_bind_valid (false),
      rtsp_service    (this /* embed_container */)
{
}

Result momentRtspInit (MomentInstance * const mt_nonnull moment_instance)
{
    logI_ (_func, "loading mod_rtsp");

    Ref<MomentRtspModule> const moment_rtsp_module = grabNewObject <MomentRtspModule> ();
    if (!moment_rtsp_module->init (moment_instance->getMomentServer())) {
        logE_ (_func, "moment_rtsp_module.init() failed");
        return Result::Failure;
    }

    if (!moment_rtsp_module->start ()) {
        logE_ (_func, "moment_rtsp_module.start() failed");
        return Result::Failure;
    }

    registerStatCounter (&stat_counter__rtsp_client);

    moment_instance->addDeletionData (moment_rtsp_module);
    return Result::Success;
}

}

#ifndef MOMENT_STATIC_MODULES
  extern "C" bool
  libMary_moduleInit (void * const moment_instance_)
          { return MomentRtsp::momentRtspInit (static_cast <MomentInstance*> (moment_instance_)); }
#endif

