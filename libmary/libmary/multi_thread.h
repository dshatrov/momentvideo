/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MULTI_THREAD__H__
#define LIBMARY__MULTI_THREAD__H__


#include <libmary/list.h>
#include <libmary/state_mutex.h>
#include <libmary/thread.h>


namespace M {

class MultiThread : public Object
{
private:
    StateMutex mutex;

    // Should be called only once. May be called again after join() completes.
    // Thread callback is reset when the thread exits.
    mt_mutex (mutex) Cb<Thread::ThreadFunc> thread_cb;

    mt_mutex (mutex) List< Ref<Thread> > thread_list;

    // Number of threads to be  spawned with spawn().
    mt_mutex (mutex) Count num_threads;
    // Number of threads currently active.
    mt_mutex (mutex) Count num_active_threads;

    mt_mutex (mutex) bool abort_spawn;

    static void wrapperThreadFunc (void *_thread_data);

public:
    // Should be called only once. May be called again after join() completes.
    mt_throws Result spawn (bool joinable);

    // Should be called once after every call to spawn(true /* joinable */).
    mt_throws Result join ();

    // Should be called only once. Has no effect after a call to spawn().
    // May be called again after join() completes.
    void setNumThreads (Count num_threads);

    // Should be called only once. May be called again after join() completes.
    // Thread callback is reset when the thread exits.
    void setThreadFunc (CbDesc<Thread::ThreadFunc> const &cb);

    MultiThread (EmbedContainer * const embed_container,
                 Count num_threads = 1,
		 CbDesc<Thread::ThreadFunc> const &thread_cb = CbDesc<Thread::ThreadFunc> ())
        : Object (embed_container),
	  thread_cb (thread_cb),
	  num_threads (num_threads),
	  num_active_threads (0),
	  abort_spawn (false)
    {}
};

}


#endif /* LIBMARY__MULTI_THREAD__H__ */

