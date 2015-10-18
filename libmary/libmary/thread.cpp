/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>

#ifdef LIBMARY_PTHREAD
  #ifdef __linux__
    #include <sys/prctl.h>
  #endif
#endif

#include <libmary/log.h>

#include <libmary/thread.h>


namespace M {

_libMary_VoidPtr
Thread::wrapperThreadFunc (_libMary_VoidPtr const _self)
{
    Thread * const self = static_cast <Thread*> (_self);

  #ifndef LIBMARY_NO_EXCEPTIONS
    try {
  #endif
        self->mutex.lock ();
        Cb<ThreadFunc> const tmp_cb = self->thread_cb;
        self->mutex.unlock ();

        tmp_cb.call_ ();
  #ifndef LIBMARY_NO_EXCEPTIONS
    } catch (...) {
        logE_ (_func, "unhandled C++ exception");
    }
  #endif

    self->mutex.lock ();
    self->thread_cb.reset ();
    self->mutex.unlock ();

    self->unref ();

  // Thread-local data is released by GPrivate dtors (see libmary_thread_local.cpp)

    return (_libMary_VoidPtr) 0;
}

mt_throws Result
Thread::spawn (bool        const joinable,
               ConstMemory const thread_name)
{
    this->ref ();

    mutex.lock ();
    assert (!spawned);
    spawned = true;
    mutex.unlock ();

  #ifdef LIBMARY_PTHREAD
    pthread_t tmp_thread;
    {
        int const res = pthread_create (&tmp_thread, NULL /* attr */, wrapperThreadFunc, this);
        if (res != 0) {
            logE_ (_func, "pthread_create() failed: ", errnoString (res));

            this->unref ();

            exc_throw (InternalException, InternalException::BackendError);
            return Result::Failure;
        }
    }

    #ifdef __linux__
      prctl (PR_SET_NAME, (unsigned long) makeString ("moment.", thread_name)->cstr(), 0, 0, 0, 0);
    #endif

    if (joinable) {
        mutex.lock ();
        this->thread = tmp_thread;
        mutex.unlock ();
    } else {
        int const res = pthread_detach (tmp_thread);
        if (res != 0)
            logE_ (_func, "pthread_detach() failed: ", errnoString (res));
    }
  #else
    GError *error = NULL;
    #ifdef LIBMARY__OLD_GTHREAD_API
      GThread * const tmp_thread = g_thread_create (wrapperThreadFunc,
                                                    this,
                                                    joinable ? TRUE : FALSE,
                                                    &error);
      this->thread = (joinable ? tmp_thread : NULL);
    #else
      GThread * const tmp_thread = g_thread_try_new (String (thread_name).cstr(),
                                                     wrapperThreadFunc,
                                                     this,
                                                     &error);

      mutex.lock ();
      if (tmp_thread) {
          if (joinable) {
              this->thread = tmp_thread;
          } else {
              g_thread_unref (tmp_thread);
              this->thread = NULL;
          }
      }
      mutex.unlock ();
    #endif

    if (!tmp_thread) {
        if (error) {
            logE_ (_func, "g_thread_create() failed: ", error->message);
            g_error_free (error);
        } else {
            logE_ (_func, "g_thread_create() failed");
        }

        this->unref ();

        exc_throw (InternalException, InternalException::BackendError);
        return Result::Failure;
    }
  #endif

    return Result::Success;
}

mt_throws Result
Thread::join ()
{
  #ifdef LIBMARY_PTHREAD
    mutex.lock ();
    pthread_t const tmp_thread = thread;
    mutex.unlock ();

    int const res = pthread_join (tmp_thread, NULL /* retval */);
    if (res != 0) {
        logE_ (_func, "pthread_join() failed: ", errnoString (res));
        return Result::Failure;
    }

    return Result::Success;
  #else
    mutex.lock ();
    assert (thread);
    GThread * const tmp_thread = thread;
    thread = NULL;
    mutex.unlock ();

    g_thread_join (tmp_thread);

    // g_thread_join() doesn't report errors, but pthread_join() may return
    // an error code. That's why Thread::join() has return value.
    return Result::Success;
  #endif
}

void Thread::setThreadFunc (CbDesc<ThreadFunc> const &cb)
{
    mutex.lock ();
    this->thread_cb = cb;
    mutex.unlock ();
}

Thread::~Thread ()
{
  #ifndef LIBMARY_PTHREAD
    mutex.lock ();
    // Make sure that join() has been called for a joinable thread.
    assert (!thread);
    mutex.unlock ();
  #endif
}

}

