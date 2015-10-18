#include <moment/libmoment.h>


using namespace Moment;

namespace MomentTest {
    Result testRtmp ();
}

using namespace MomentTest;

int main (void)
{
    int exit_code = 0;

    libMaryInit ();
    logD_ (_func, "Hello, World!");

    setGlobalLogLevel (LogLevel::All);

    if (!testRtmp()) {
        logE_ (_func, "testRtmp() FAILED");
        exit_code = EXIT_FAILURE;
    }

    logD_ (_func, "done");
    return exit_code;
}

