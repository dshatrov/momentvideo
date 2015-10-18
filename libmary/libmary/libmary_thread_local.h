/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LIBMARY_THREAD_LOCAL__H__
#define LIBMARY__LIBMARY_THREAD_LOCAL__H__


#include <libmary/types.h>
#include <time.h>

#ifdef LIBMARY_PTHREAD
  #include <pthread.h>
#endif

#ifdef LIBMARY_ENABLE_MWRITEV
  #include <sys/uio.h>
#endif

#include <libmary/exception_buffer.h>


#ifdef LIBMARY__OLD_GTHREAD_API
  #ifndef LIBMARY_TLOCAL
    #define LIBMARY__TLOCAL_GPRIVATE    (_libMary_tlocal_gprivate)
  #endif
  #define LIBMARY__TLOCAL_GPRIVATE_DTOR (_libMary_tlocal_gprivate_dtor)
#else
  #ifndef LIBMARY_TLOCAL
    #define LIBMARY__TLOCAL_GPRIVATE    (&_libMary_tlocal_gprivate)
  #endif
  #define LIBMARY__TLOCAL_GPRIVATE_DTOR (&_libMary_tlocal_gprivate_dtor)
#endif


#include <libmary/object.h>


namespace M {

class Exception;

class CodeReferenced;

#ifdef LIBMARY_ENABLE_MWRITEV
// DeferredConnectionSender's mwritev data.
class LibMary_MwritevData
{
  public:
    bool           initialized;
    int           *fds;
    struct iovec **iovs;
    struct iovec  *iovs_heap;
    int           *num_iovs;
    int           *res;

    LibMary_MwritevData ()
	: initialized (false)
    {}
};
#endif

class DeletionContext;

class LibMary_ThreadLocal
{
  public:
    DeletionContext *deletion_ctx;

    Object *deletion_queue;
    bool deletion_queue_processing;

    Count state_mutex_counter;

    Ref<ExceptionBuffer> exc_buffer;
    Exception *exc;
    IntrusiveList<ExceptionBuffer> exc_block_stack;
    IntrusiveList<ExceptionBuffer> exc_free_stack;
    Size exc_free_stack_size;

    Object::Shadow *last_coderef_container_shadow;

    char *strerr_buf;
    Size strerr_buf_size;

  // Time-related data fields

    Time time_seconds;
    Time time_microseconds;
    Time unixtime;

    Time time_log_frac;

    struct tm localtime;
    Time saved_unixtime;
    // Saved monotonic clock value in seconds.
    Time saved_monotime;

    char timezone_str [5];

  #ifdef LIBMARY_PLATFORM_WIN32
    DWORD prv_win_time_dw;
    Time win_time_offs;
  #endif

  #ifdef LIBMARY_ENABLE_MWRITEV
    LibMary_MwritevData mwritev;
  #endif

  #ifndef LIBMARY_GLIB
    bool     rand_initialized;
    unsigned rand_state;
  #endif

     LibMary_ThreadLocal ();
    ~LibMary_ThreadLocal ();
};

#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY_PTHREAD
    extern pthread_key_t _libMary_tlocal_key;
  #else
    #ifdef LIBMARY__OLD_GTHREAD_API
      #ifndef LIBMARY_TLOCAL
        extern GPrivate *_libMary_tlocal_gprivate;
      #endif
      extern GPrivate *_libMary_tlocal_gprivate_dtor;
    #else
      #ifndef LIBMARY_TLOCAL
        extern GPrivate _libMary_tlocal_gprivate;
      #endif
      extern GPrivate _libMary_tlocal_gprivate_dtor;
    #endif
  #endif
#endif

#ifndef LIBMARY_MT_SAFE
  extern LibMary_ThreadLocal *_libMary_tlocal;
  #define libMary_getThreadLocal() (_libMary_tlocal)
#else
  #ifdef LIBMARY_TLOCAL
    extern LIBMARY_TLOCAL LibMary_ThreadLocal *_libMary_tlocal;

    static inline mt_nonnull LibMary_ThreadLocal* libMary_getThreadLocal()
    {
        LibMary_ThreadLocal *tlocal = _libMary_tlocal;
        if (tlocal)
            return tlocal;

        tlocal = new (std::nothrow) LibMary_ThreadLocal;
        assert (tlocal);
        _libMary_tlocal = tlocal;
        #ifdef LIBMARY_PTHREAD
          int const res = pthread_setspecific (_libMary_tlocal_key, tlocal);
          assert (res == 0);
        #else
          g_private_set (LIBMARY__TLOCAL_GPRIVATE_DTOR, tlocal);
        #endif
        return tlocal;
    }
  #else
    static inline mt_nonnull LibMary_ThreadLocal* libMary_getThreadLocal()
    {
        LibMary_ThreadLocal *tlocal =
              #ifdef LIBMARY_PTHREAD
                static_cast <LibMary_ThreadLocal*> (pthread_getspecific (_libMary_tlocal_key));
              #else
                static_cast <LibMary_ThreadLocal*> (g_private_get (LIBMARY__TLOCAL_GPRIVATE));
              #endif
        if (tlocal)
            return tlocal;

        tlocal = new (std::nothrow) LibMary_ThreadLocal;
        assert (tlocal);
        #ifdef LIBMARY_PTHREAD
          int const res = pthread_setspecific (_libMary_tlocal_key, tlocal);
          assert (res == 0);
        #else
          g_private_set (LIBMARY__TLOCAL_GPRIVATE_DTOR, tlocal);
          g_private_set (LIBMARY__TLOCAL_GPRIVATE, tlocal);
        #endif
        return tlocal;
    }
  #endif
#endif

void libMary_threadLocalInit ();

void libMary_releaseThreadLocalForMainThread ();

}


#endif /* LIBMARY__LIBMARY_THREAD_LOCAL__H__ */

