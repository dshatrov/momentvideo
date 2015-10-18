#include <moment/libmoment.h>


using namespace Moment;


static Mp4VodSource::Frontend mp4_vod_frontend = {
};

Result test ()
{
    Ref<PagePool> const page_pool = grab (new (std::nothrow) PagePool (NULL /* embed_container */));
    page_pool->init (4096 /* page_size */,
                     0    /* min_pages */,
                     true /* delete_immediately */,
                     NULL /* timers */,
                     0    /* delete_interval_microsec */,
                     0    /* delete_bulk_size */);

    Ref<ServerApp> const server_app = grab (new (std::nothrow) ServerApp (NULL /* embed_container */));
    Ref<Mp4VodSource> const mp4_vod_source = grab (new (std::nothrow) Mp4VodSource (NULL /* embed_container */));

    mp4_vod_source->init ("test.mp4", page_pool, CbDesc<Mp4VodSource::Frontend> (&mp4_vod_frontend, NULL, NULL));
    if (!mp4_vod_source->open ()) {
        logE_ (_func, "mp4_vod_source.open() failed");
        return Result::Failure;
    }

    if (!mp4_vod_source->readSegmentHeader (1000000000ULL /* begin_nanosec */,
                                            2000000000ULL /* end_nanosec */,
                                            2000000       /* size_limit */))
    {
        logE_ (_func, "mp4_vod_source.readSegmentHeader() failed");
        return Result::Failure;
    }

    return Result::Success;
}

int main (void)
{
    libMaryInit ();
    if (!test ())
        return EXIT_FAILURE;
}

