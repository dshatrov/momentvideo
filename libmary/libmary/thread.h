/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__THREAD__H__
#define LIBMARY__THREAD__H__


#include <libmary/types.h>

#ifdef LIBMARY_PTHREAD
  #include <pthread.h>
#else
  #include <glib.h>
#endif

#include <libmary/exception.h>
#include <libmary/cb.h>
#include <libmary/state_mutex.h>


namespace M {

class Thread : public Object
{
private:
    StateMutex mutex;

public:
    typedef void ThreadFunc (void *cb_data);

private:
    // 'thread_cb' gets reset when the thread exits.
    mt_mutex (mutex) Cb<ThreadFunc> thread_cb;
  #ifdef LIBMARY_PTHREAD
    mt_mutex (mutex) pthread_t thread;
  #else
    mt_mutex (mutex) GThread *thread;
  #endif
    mt_mutex (mutex) bool spawned;

    static _libMary_VoidPtr wrapperThreadFunc (_libMary_VoidPtr _self);

public:
    // Should be called only once. May be called again after join() completes.
    mt_throws Result spawn (bool        joinable,
                            ConstMemory thread_name = ConstMemory ("libmary-thread"));

    // Should be called once after every call to spawn(true /* joinable */).
    mt_throws Result join ();

    // Should be called only once. May be called again after join() completes.
    // Thread callback is reset when the thread exits.
    void setThreadFunc (CbDesc<ThreadFunc> const &cb);

    Thread (EmbedContainer * const embed_container,
            CbDesc<ThreadFunc> const &thread_cb = CbDesc<ThreadFunc> ())
        : Object    (embed_container),
          thread_cb (thread_cb),
        #ifndef LIBMARY_PTHREAD
          thread    (NULL),
        #endif
          spawned   (false)
    {}

    ~Thread ();
};

}


#endif /* LIBMARY__THREAD__H__ */

