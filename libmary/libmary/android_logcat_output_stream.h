#ifndef LIBMARY__ANDROID_LOGCAT_OUTPUT_STREAM__H__
#define LIBMARY__ANDROID_LOGCAT_OUTPUT_STREAM__H__


#include <libmary/types_base.h>

#ifdef LIBMARY_PLATFORM_ANDROID
#include <libmary/output_stream.h>


namespace M {

class AndroidLogcatOutputStream : public OutputStream
{
  public:
    mt_iface (OutputStream)
      mt_throws Result write (ConstMemory  mem,
                              Size        *ret_nwritten);

      mt_throws Result flush () { return Result::Success; }
    mt_iface_end
};

}
#endif /* LIBMARY_PLATFORM_ANDROID */


#endif /* LIBMARY__ANDROID_LOGCAT_OUTPUT_STREAM__H__ */

