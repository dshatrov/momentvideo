/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__FAST_MUTEX__H__
#define LIBMARY__FAST_MUTEX__H__


#include <libmary/types.h>
#ifdef LIBMARY_MT_SAFE
  #include <libmary/cond.h>
#endif


namespace M {

#ifdef LIBMARY_MT_SAFE
// This mutex implementation is presumably faster (1 atomic op for lock/unlock)
// in non-contended cases, and slower in contended ones.
// FastMutex is used for Object::Shadow::shadow_mutex, which is assumed to have
// very low contention. This speeds up working with weak references.
//
class FastMutex
{
private:
    Atomic<Count> lock_cnt;

    bool unlocked;

    Mutex mutex;
    Cond cond;

public:
    void lock ()
    {
        if (lock_cnt.fetch_add (1, std::memory_order_acquire) == 0)
            return;

        mutex.lock ();
        while (!unlocked) {
            cond.wait (mutex);
        }
        unlocked = false;
        mutex.unlock ();
    }

    void unlock ()
    {
        if (lock_cnt.fetch_sub (1, std::memory_order_release) == 1)
            return;

        mutex.lock ();
        unlocked = true;
        cond.signal ();
        mutex.unlock ();
    }

    FastMutex ()
        : unlocked (false)
    {}
};
#else
class FastMutex
{
public:
    void lock   () {}
    void unlock () {}
};
#endif

}


#endif /* LIBMARY__FAST_MUTEX__H__ */

