#include <libmary/libmary.h>


using namespace M;


static LogGroup libMary_logGroup_mytest ("mytest", LogLevel::All);


int main (void)
{
    libMaryInit ();

    log_ (LogLevel::Info, "Hello, World!");
    logD (mytest, "Starting!");
    logD (mytest, "Habahaba, ", "and this is just ", newString ("The beginning"));
    logE_ ("Muhahahaha! -> ", 13, " ", 42);

    {
        Byte buf [77];
        for (unsigned i = 0; i < sizeof (buf); ++i)
            buf [i] = i;

        logHex_ (LogLevel::Debug, ConstMemory::forObject (buf), _func);
    }
}

