/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/libmoment.h>


using namespace M;

namespace Moment {

void
dumpGstBufferFlags (GstBuffer * const buffer)
{
    Uint32 flags = (Uint32) GST_BUFFER_FLAGS (buffer);
    if (flags != GST_BUFFER_FLAGS (buffer))
	log__ (LogLevel::Debug, _func, "flags do not fit into Uint32");

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_READONLY)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_READONLY");
	flags ^= GST_BUFFER_FLAG_READONLY;
    }
#endif

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA4)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_MEDIA4");
	flags ^= GST_BUFFER_FLAG_MEDIA4;
    }
#endif

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_PREROLL)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_PREROLL");
	flags ^= GST_BUFFER_FLAG_PREROLL;
    }
#endif

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_DISCONT");
	flags ^= GST_BUFFER_FLAG_DISCONT;
    }

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_IN_CAPS");
	flags ^= GST_BUFFER_FLAG_IN_CAPS;
    }
#endif

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_GAP)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_GAP");
	flags ^= GST_BUFFER_FLAG_GAP;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_DELTA_UNIT");
	flags ^= GST_BUFFER_FLAG_DELTA_UNIT;
    }

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA1)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_MEDIA1");
	flags ^= GST_BUFFER_FLAG_MEDIA1;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA2)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_MEDIA2");
	flags ^= GST_BUFFER_FLAG_MEDIA2;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA3)) {
	log__ (LogLevel::Debug, _func, "GST_BUFFER_FLAG_MEDIA3");
	flags ^= GST_BUFFER_FLAG_MEDIA3;
    }
#endif

    if (flags)
	log__ (LogLevel::Debug, _func, "Extra flags: 0x", fmt_hex, flags);
}

void
libMomentGstInit (ConstMemory const gst_debug_str)
{
// gst_is_initialized() is new API.
//    if (!gst_is_initialized())

    if (gst_debug_str.len() > 0) {
        String const str (gst_debug_str);

        int argc = 2;
        char* argv [] = {
            (char*) "moment",
            (char*) str.cstr(),
            NULL
        };

        char **argv_ = argv;
        gst_init (&argc, &argv_);
    } else {
        gst_init (NULL /* args */, NULL /* argv */);
    }
}

}

