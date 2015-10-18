/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>
#include <moment/libmoment.h>
#include <moment/moment_instance.h>


using namespace Moment;

namespace MomentTest {

Result momentTestInit (MomentInstance * const mt_nonnull moment_instance)
{
    logD_ (_func, "Initializing mod_test");

    Ref<MomentServer>    const moment_server = moment_instance->getMomentServer ();
    Ref<MConfig::Config> const config        = moment_server->getConfigManager()->getConfig();

    {
        bool enable = false;

        ConstMemory const opt_name = "mod_test/enable";
        if (!configGetBoolean (config, opt_name, &enable, enable, _func))
            return Result::Failure;

        if (!enable) {
            logI_ (_func, "Test module (mod_test) is not enabled. "
                   "Set \"", opt_name, "\" option to \"y\" to enable.");
            return Result::Success;
        }
    }

    TestStreamGenerator::Options opts;

    if (   !configGetUint64_  (config, "mod_test/frame_duration",    &opts.frame_duration,    _func)
        || !configGetUint64_  (config, "mod_test/keyframe_interval", &opts.keyframe_interval, _func)
        || !configGetUint64_  (config, "mod_test/frame_size",        &opts.frame_size,        _func)
        || !configGetUint64_  (config, "mod_test/start_timestamp",   &opts.start_timestamp,   _func)
        || !configGetUint64_  (config, "mod_test/burst_width",       &opts.burst_width,       _func)
        || !configGetBoolean_ (config, "mod_test/same_pages",        &opts.use_same_pages,    _func))
    {
        return Result::Failure;
    }

    ConstMemory const stream_name = config->getString_default ("mod_test/stream_name", "test");

    Ref<MediaStream> const stream =
            grabNewObject <MediaStream> (
                    moment_server->getServerApp()->getServerContext()->getMainThreadContext()->getDeferredProcessor());
    moment_server->getStreamManager()->addStream (stream, stream_name);

    Ref<TestStreamGenerator> const test_stream_generator = grabNewObject <TestStreamGenerator> ();
    test_stream_generator->init (moment_server->getPagePool(),
                                 moment_server->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                                 stream,
                                 &opts);
    test_stream_generator->start ();

    moment_instance->addDeletionData (test_stream_generator);
    return Result::Success;
}

}

#ifndef MOMENT_STATIC_MODULES
  extern "C" bool
  libMary_moduleInit (void * const moment_instance_)
          { return MomentTest::momentTestInit (static_cast <MomentInstance*> (moment_instance_)); }
#endif

