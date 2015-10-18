#include <libmary/libmary.h>


namespace libMary_Test {

using namespace M;

static void verifyEncode (ConstMemory const from,
                          ConstMemory const to)
{
    Byte buf [base64EncodedBufferSize (from.len())];
    Size const len = base64Encode (from, buf);
    logD_ (_func, "from \"", from, "\", to \"", to, "\", result \"", ConstMemory (buf, len), "\"");
    assert_hard (equal (ConstMemory (buf, len), to));
}

static void verifyDecode (ConstMemory const from,
                          ConstMemory const to)
{
    Byte buf [base64DecodedBufferSize (from.len())];
    Size const len = base64Decode (from, buf);
    logD_ (_func, "from \"", from, "\", to \"", to, "\", result \"", ConstMemory (buf, len), "\"");
    assert_hard (equal (ConstMemory (buf, len), to));
}

static void verifyBase64 (ConstMemory const from,
                          ConstMemory const to)
{
    verifyEncode (from, to);
    verifyDecode (to, from);
}

void testBase64 ()
{
    verifyBase64 ("",       "");
    verifyBase64 ("f",      "Zg==");
    verifyBase64 ("fo",     "Zm8=");
    verifyBase64 ("foo",    "Zm9v");
    verifyBase64 ("foob",   "Zm9vYg==");
    verifyBase64 ("fooba",  "Zm9vYmE=");
    verifyBase64 ("foobar", "Zm9vYmFy");
}

}

