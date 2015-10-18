#include <libmary/android_logcat_output_stream.h>

#ifdef LIBMARY_PLATFORM_ANDROID
#include <android/log.h>


namespace M {
    
mt_throws Result
AndroidLogcatOutputStream::write (ConstMemory   const mem,
                                  Size        * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = mem.len();

    __android_log_print (ANDROID_LOG_INFO,
                         "---DEBUG",
                         "%.*s",
                         (int) mem.len(),
                         (char const *) mem.buf());

    return Result::Success;
}

}
#endif /* LIBMARY_PLATFORM_ANDROID */

