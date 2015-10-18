#include <moment/libmoment.h>


using namespace Moment;


Result test ()
{
    Ref<PagePool> const page_pool = grab (new (std::nothrow) PagePool (NULL /* embed_container */));
    page_pool->init (4096 /* page_size */,
                     0    /* min_pages */,
                     true /* delete_immediately */,
                     NULL /* timers */,
                     0    /* delete_interval_microsec */,
                     0    /* delete_bulk_size */);

    Mp4Reader mp4_reader;
    mp4_reader.init (page_pool);
    mp4_reader.open ("test.mp4");

    mp4_reader.seek (1000000);

    for (;;) {
        VodReader::ReadMessage read_msg;
        MediaMessage *msg = NULL;
        VodReader::ReadResult const res = mp4_reader.readNextFrame (&read_msg,
                                                                    &msg,
//                                                                    false /* stop_time_enabled */,
//                                                                    0     /* stop_time_microsec */);
                                                                    true    /* stop_time_enabled */,
                                                                    2000000 /* stop_time_microsec */);
        logD_ (_func, "res: ", res);
        if (res == VodReader::ReadResult::Error) {
            assert (!msg);
            logE_ (_func, "readNextFrame() failed");
            return Result::Failure;
        } else
        if (res == VodReader::ReadResult::NoMoreFrames) {
            assert (!msg);
            break;
        } else
        if (res == VodReader::ReadResult::StopTimeReached) {
            assert (!msg);
            break;
        }
        assert (res == VodReader::ReadResult::Normal);
        assert (msg);

        logD_ (_func, "msg: ", *msg);
        msg->release ();
    }

    return Result::Success;
}

int main (void)
{
    libMaryInit ();
    if (!test ())
        return EXIT_FAILURE;
}

