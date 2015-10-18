/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>
#include <moment/moment_instance.h>

// libavcodec headers are in pure C
extern "C" {
  #include <libavcodec/avcodec.h>
}

#include <moment-thumb/thumb_service.h>


using namespace MomentThumb;

namespace {
class MomentThumbModule : public Object
{
  private:
    Embed<ThumbService> thumb_service;

  public:
    mt_const Result init (MomentServer    * mt_nonnull moment,
                          MConfig::Config * mt_nonnull config);

    MomentThumbModule (EmbedContainer *embed_container);
};
}

mt_const Result
MomentThumbModule::init (MomentServer    * const mt_nonnull moment,
                         MConfig::Config * const mt_nonnull config)
{
    Uint64 thumb_interval_millisec = 60000;
    if (!configGetUint64 (config, "mod_thumb/thumb_interval", &thumb_interval_millisec, thumb_interval_millisec, _func))
        return Result::Failure;

    thumb_service->init (moment, thumb_interval_millisec);

    return Result::Success;
}

MomentThumbModule::MomentThumbModule (EmbedContainer * const embed_container)
    : Object        (embed_container),
      thumb_service (this /* embed_container */)
{
}

namespace MomentThumb {

Result momentThumbInit (MomentInstance * const mt_nonnull moment_instance)
{
    Ref<MomentServer> const moment_server = moment_instance->getMomentServer();
    Ref<MConfig::Config> const config = moment_server->getConfigManager()->getConfig();

    {
        bool enable = false;
        ConstMemory const opt_name = "mod_thumb/enable";
        if (!configGetBoolean_ (config, opt_name, &enable, _func))
            return Result::Failure;

        if (!enable) {
            logI_ (_func, "mod_thumb module is not enabled");
            return Result::Success;
        }
    }

    logI_ (_func, "initializing mod_thumb");

    avcodec_register_all ();

    Ref<MomentThumbModule> const moment_thumb_module = grabNewObject <MomentThumbModule> ();
    if (!moment_thumb_module->init (moment_server, config)) {
        logE_ (_func, "moment_thumb_module->init() failed");
        return Result::Failure;
    }

    moment_instance->addDeletionData (moment_thumb_module);
    return Result::Success;
}

}

#ifndef MOMENT_STATIC_MODULES
  extern "C" bool
  libMary_moduleInit (void * const moment_instance_)
          { return MomentThumb::momentThumbInit (static_cast <MomentInstance*> (moment_instance_)); }
#endif

