#include <libmary/libmary.h>


namespace libMary_Test {

    void testSender ();
    void testReferenced ();
    void testObject ();
    void testRef ();
    void testLogRotateStream ();
    void testBase64 ();

}


using namespace M;
using namespace libMary_Test;

int main (void)
{
    libMaryInit ();

    testSender ();
    testReferenced ();
    testObject ();
    testRef ();
    testLogRotateStream ();
    testBase64 ();

    libMaryRelease ();
}

