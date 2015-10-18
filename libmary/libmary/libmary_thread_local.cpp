/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>


//#include <cstdio>

#include <libmary/exception.h>
#include <libmary/log.h>

#include <libmary/libmary_thread_local.h>


namespace M {

#ifdef LIBMARY_MT_SAFE
  static void tlocal_destructor (_libMary_VoidPtr _tlocal);

  #ifdef LIBMARY_PTHREAD
    pthread_key_t _libMary_tlocal_key;
  #else
    #ifdef LIBMARY__OLD_GTHREAD_API
      #ifndef LIBMARY_TLOCAL
        GPrivate *_libMary_tlocal_gprivate = NULL;
      #endif
      GPrivate *_libMary_tlocal_gprivate_dtor = NULL;
    #else
      #ifndef LIBMARY_TLOCAL
        GPrivate _libMary_tlocal_gprivate;
      #endif
      GPrivate _libMary_tlocal_gprivate_dtor = G_PRIVATE_INIT (tlocal_destructor);
    #endif
  #endif

  #ifdef LIBMARY_TLOCAL
    LIBMARY_TLOCAL LibMary_ThreadLocal *_libMary_tlocal = NULL;
  #endif
  static LibMary_ThreadLocal *_libMary_main_tlocal = NULL;
#else
  LibMary_ThreadLocal *_libMary_tlocal = NULL;
#endif

LibMary_ThreadLocal::LibMary_ThreadLocal ()
    : deletion_ctx (NULL),

      deletion_queue (NULL),
      deletion_queue_processing (false),
      state_mutex_counter (0),

      exc (NULL),
      exc_free_stack_size (0),

      last_coderef_container_shadow (NULL),

      time_seconds (0),
      time_microseconds (0),
      unixtime (0),

      time_log_frac (0),

      saved_unixtime (0),
      saved_monotime (0)

    #ifdef LIBMARY_PLATFORM_WIN32
      ,
      prv_win_time_dw (0),
      win_time_offs (0)
    #endif

    #ifndef LIBMARY_GLIB
      ,
      rand_initialized (false),
      rand_state (0)
    #endif
{
    exc_buffer = grab (new (std::nothrow) ExceptionBuffer (LIBMARY__EXCEPTION_BUFFER_SIZE));

    memset (&localtime, 0, sizeof (localtime));
    memset (timezone_str, ' ', sizeof (timezone_str));

    strerr_buf_size = 4096;
    strerr_buf = new (std::nothrow) char [strerr_buf_size];
    assert (strerr_buf);
}

LibMary_ThreadLocal::~LibMary_ThreadLocal ()
{
  // Note: This dtor should not call any code which requires valid tlocal.

    // Exceptions cleanup
    while (!exc_block_stack.isEmpty() || !exc_free_stack.isEmpty()) {
        {
            IntrusiveList<ExceptionBuffer>::iterator iter (exc_block_stack);
            while (!iter.done()) {
                ExceptionBuffer * const exc_buf = iter.next ();
                delete exc_buf;
            }
            exc_block_stack.clear ();
        }

        {
            IntrusiveList<ExceptionBuffer>::iterator iter (exc_free_stack);
            while (!iter.done()) {
                ExceptionBuffer * const exc_buf = iter.next ();
                delete exc_buf;
            }
            exc_free_stack.clear ();
        }

        if (exc_buffer->getException())
            exc_none ();
        else
            exc_buffer = NULL;
    }

    delete[] strerr_buf;
}

#ifdef LIBMARY_MT_SAFE
static void
tlocal_destructor (_libMary_VoidPtr const _tlocal)
{
    logD_ (_func, "tlocal ", (UintPtr) _tlocal);

  #ifndef LIBMARY_TLOCAL
    #ifndef LIBMARY_PTHREAD
      // All gprivates are reset to NULL by glib/pthreads before tlocal_destructor()
      // is called. We restore the right value for tlocal gprivate, which is safe
      // since it doesn't have an associated destructor callback.
      g_private_set (LIBMARY__TLOCAL_GPRIVATE, _tlocal);
    #endif
  #endif

    // Exception dtors may call arbitrary code, so we're
    // clearing exceptions first.
    exc_none ();

    if (LibMary_ThreadLocal * const tlocal = static_cast <LibMary_ThreadLocal*> (_tlocal))
        delete tlocal;

  #ifdef LIBMARY_TLOCAL
    _libMary_tlocal = NULL;
  #endif
}
#endif

void
libMary_threadLocalInit ()
{
  #ifdef LIBMARY_MT_SAFE
    _libMary_main_tlocal = new (std::nothrow) LibMary_ThreadLocal;
    assert (_libMary_main_tlocal);

    #ifdef LIBMARY_PTHREAD
      {
          int const res = pthread_key_create (&_libMary_tlocal_key, tlocal_destructor);
          assert (res == 0);
      }

      {
          int const res = pthread_setspecific (_libMary_tlocal_key, _libMary_main_tlocal);
          assert (res == 0);
      }
    #else
      #ifdef LIBMARY__OLD_GTHREAD_API
        _libMary_tlocal_gprivate_dtor = g_private_new (tlocal_destructor);
        #ifndef LIBMARY_TLOCAL
          _libMary_tlocal_gprivate = g_private_new (NULL /* notify */);
        #endif
      #endif

      g_private_set (LIBMARY__TLOCAL_GPRIVATE, _libMary_main_tlocal);
    #endif
  #else
      _libMary_tlocal = new (std::nothrow) LibMary_ThreadLocal;
      assert (_libMary_tlocal);
  #endif
}

void
libMary_releaseThreadLocalForMainThread ()
{
    // Exception dtors may call arbitrary code, so we're
    // clearing exceptions first.
    exc_none ();

  #if defined LIBMARY_MT_SAFE
    delete _libMary_main_tlocal;
    _libMary_main_tlocal = NULL;
  #else
    delete _libMary_tlocal;
    _libMary_tlocal = NULL;
  #endif
}

}

