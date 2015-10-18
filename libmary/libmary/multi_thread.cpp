/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>

#include <libmary/multi_thread.h>


namespace M {

namespace {
class ThreadData : public Referenced
{
public:
    Cb<Thread::ThreadFunc> thread_cb;
};
}

void
MultiThread::wrapperThreadFunc (void *_self)
{
    MultiThread * const self = static_cast <MultiThread*> (_self);

  {
    self->mutex.lock ();
    if (self->abort_spawn) {
        self->mutex.unlock ();
        goto _return;
    }

    Cb<Thread::ThreadFunc> const tmp_cb = self->thread_cb;
    self->mutex.unlock ();

    tmp_cb.call_ ();

    self->mutex.lock ();
    assert (self->num_active_threads > 0);
    --self->num_active_threads;
    if (self->num_active_threads == 0) {
        self->thread_cb.reset ();
    }
    self->mutex.unlock ();
  }

_return:
    self->unref ();
}

mt_throws Result
MultiThread::spawn (bool const joinable)
{
    mutex.lock ();

    assert (num_active_threads == 0);
    for (Count i = 0; i < num_threads; ++i) {
        this->ref ();

        Ref<Thread> const thread =
                grabNewObject <Thread> (
                        CbDesc<Thread::ThreadFunc> (
                                wrapperThreadFunc, this, NULL /* we explicitly ref() ourselves */));

        thread_list.append (thread);
        if (!thread->spawn (joinable)) {
            abort_spawn = true;
            thread_cb.reset ();
            mutex.unlock ();

            this->unref ();
            return Result::Failure;
        }
    }
    num_active_threads = num_threads;

    mutex.unlock ();

    return Result::Success;
}

// TODO Never fails?
mt_throws Result
MultiThread::join ()
{
    mutex.lock ();

    assert (num_active_threads <= num_threads);
    for (Count i = 0; i < num_threads; ++i) {
        assert (!thread_list.isEmpty());
        Ref<Thread> const thread = thread_list.getFirst();
        thread_list.remove (thread_list.getFirstElement());
        mutex.unlock ();

        if (!thread->join ())
            logE_ (_func, "Thread::join() failed: ", exc->toString());

        mutex.lock ();
    }
    assert (num_active_threads == 0);

    mutex.unlock ();

    return Result::Success;
}

void
MultiThread::setNumThreads (Count const num_threads)
{
    mutex.lock ();
    this->num_threads = num_threads;
    mutex.unlock ();
}

void
MultiThread::setThreadFunc (CbDesc<Thread::ThreadFunc> const &cb)
{
    mutex.lock ();
    this->thread_cb = cb;
    mutex.unlock ();
}

}

