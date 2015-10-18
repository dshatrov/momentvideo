#include <moment/moment_instance.h>
#include <moment-rtmp/librtmp.h>


using namespace Moment;

namespace MomentTest { Result momentTestInit (MomentInstance * mt_nonnull moment_instance); }
namespace MomentRtmp { Result momentRtmpInit (MomentInstance * mt_nonnull moment_instance); }

namespace {
class TestRtmp : public Object
{
  public:
    enum_beg (TestKind)
        enum_values (ConnectionRefused, GotVideo)
    enum_end (TestKind)

  private:
    StateMutex mutex;

    TestKind const test_kind;

    mt_const Ref<MomentInstance> moment_instance;

    mt_const Ref<MediaStream> stream;
    mt_const Ref<RtmpClient>  rtmp_client;

    mt_mutex (mutex) Result test_result;

    static void testTimeoutTimerTick (void *_self);

    mt_iface ( FetchConnection::FetchConnectionFrontend)
      static FetchConnection::FetchConnectionFrontend const rtmp_client_frontend;

      static void rtmpClientDisconnected (DisconnectReason  disconnect_reason,
                                          void             *_self);

      static void rtmpClientGotVideo (bool  got_video,
                                      void *_self);
    mt_iface_end

    mt_iface ( MomentInstance)
      static MomentInstance::Events const moment_instance_events;
      static void momentInstance_started (void *_self);
    mt_iface_end

    void finishTest ();
    void abortTest  ();

  public:
    Result run ();

    TestRtmp (EmbedContainer * const embed_container,
              TestKind         const test_kind)
        : Object      (embed_container),
          test_kind   (test_kind),
          test_result (Result::Failure)
    {}
};
}

void
TestRtmp::testTimeoutTimerTick (void * const _self)
{
    TestRtmp * const self = static_cast <TestRtmp*> (_self);
    logD_ (_self_func_);

    self->abortTest ();
}

FetchConnection::FetchConnectionFrontend const TestRtmp::rtmp_client_frontend (
    rtmpClientDisconnected,
    rtmpClientGotVideo
);

void
TestRtmp::rtmpClientDisconnected (DisconnectReason   const disconnect_reason,
                                  void             * const _self)
{
    TestRtmp * const self = static_cast <TestRtmp*> (_self);
    logD_ (_self_func, "disconnect_reason ", disconnect_reason);

    if (self->test_kind == TestKind::ConnectionRefused) {
        if (disconnect_reason == DisconnectReason::CouldNotEstablishUnderlyingConnection)
            self->finishTest ();
    }
}

void
TestRtmp::rtmpClientGotVideo (bool   const got_video,
                              void * const _self)
{
    TestRtmp * const self = static_cast <TestRtmp*> (_self);
    logD_ (_self_func, "got_video ", got_video);

    if (self->test_kind == TestKind::GotVideo) {
        if (got_video) {
            self->finishTest ();
            return;
        }
    }

    self->abortTest ();
}

MomentInstance::Events const TestRtmp::moment_instance_events (
    momentInstance_started
);

void
TestRtmp::momentInstance_started (void * const _self)
{
    TestRtmp * const self = static_cast <TestRtmp*> (_self);
    logD_ (_self_func_);

    IpAddress server_addr;
    assert_hard (setIpAddress ("127.0.0.1", 1935, &server_addr));

    self->rtmp_client = grabNewObject <RtmpClient> ();
    {
        RtmpClient::RtmpClientConfig rtmp_client_config;
        rtmp_client_config.no_video_check_interval_millisec = 100;
        rtmp_client_config.no_video_timeout_millisec        = 300;

        self->rtmp_client->init (self->moment_instance->getServerApp()->getMainThreadContext(),
                                 self->moment_instance->getPagePool(),
                                 self->stream,
                                 server_addr,
                                 /*app_name=*/    "live",
                                 /*stream_name=*/ "test",
                                 rtmp_client_config,
                                 CbDesc<FetchConnection::FetchConnectionFrontend> (&rtmp_client_frontend, self, self));
    }

    if (!self->rtmp_client->start ()) {
        logE_ (_self_func, "rtmp_client.start() failed");
        self->abortTest ();
        return;
    }
}

void
TestRtmp::finishTest ()
{
    mutex.lock ();
    test_result = Result::Success;
    mutex.unlock ();

    moment_instance->exit (/*reason=*/ ConstMemory());
}

void
TestRtmp::abortTest ()
{
    mutex.lock ();
    test_result = Result::Failure;
    mutex.unlock ();

    moment_instance->exit (/*reason=*/ ConstMemory());
}

Result
TestRtmp::run ()
{
    logD_ (_this_func_);

    moment_instance = grabNewObject <MomentInstance> ();
    moment_instance->init (
            MomentInstance::ConfigDesc_Memory (
                "mod_test { enable=true; }"),
            MomentInstance::LogParams (),
            MomentInstance::ControlParams ());

    stream = grabNewObject <MediaStream> (
                     moment_instance->getMomentServer()->getServerApp()->
                             getServerContext()->getMainThreadContext()->getDeferredProcessor());

    if (!MomentTest::momentTestInit (moment_instance)) {
        logE_ (_this_func, "momentTestInit() failed");
        return Result::Failure;
    }

    if (test_kind != TestKind::ConnectionRefused) {
        if (!MomentRtmp::momentRtmpInit (moment_instance)) {
            logE_ (_this_func, "momentRtmpInit() failed");
            return Result::Failure;
        }
    }

    moment_instance->getEventInformer()->subscribe (
            CbDesc<MomentInstance::Events> (&moment_instance_events, this, this));

    moment_instance->getServerApp()->getMainThreadContext()->getTimers()->addTimer (
            CbDesc<Timers::TimerCallback> (testTimeoutTimerTick, this, this),
            /*time_microseconds=*/ 10000000,
            /*periodical=*/        false);

    if (!moment_instance->run ()) {
        logE_ (_this_func, "moment_instance.run() failed");
        return Result::Failure;
    }

    mutex.lock ();
    Result const tmp_res = test_result;
    mutex.unlock ();
    return tmp_res;
}

namespace MomentTest {

Result testRtmp ()
{
    if (!grabNewObject <TestRtmp> (TestRtmp::TestKind::ConnectionRefused)->run ()) {
        logE_ (_func, "ConnectionRefused test failed");
        return Result::Failure;
    }

    if (!grabNewObject <TestRtmp> (TestRtmp::TestKind::GotVideo)->run ()) {
        logE_ (_func, "GotVideo test failed");
        return Result::Failure;
    }

    return Result::Success;
}

}

