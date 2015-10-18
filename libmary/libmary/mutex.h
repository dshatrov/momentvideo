/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MUTEX__H__
#define LIBMARY__MUTEX__H__


#include <libmary/types.h>

#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY_PTHREAD
    #include <pthread.h>
  #else
    #include <glib.h>
  #endif
#endif


namespace M {

class Mutex;
// RawMutex is a better name to distinct from StateMutex.
typedef Mutex RawMutex;

// TODO Use C++11 mutexes and conditions.

/*c */
class Mutex
{
    LIBMARY_NO_COPY (Mutex)

  #ifdef LIBMARY_MT_SAFE
    #if defined (LIBMARY_PTHREAD)
      // Glib's mutexes and conds are crappy when it comes to performance,
      // especially after API change in 2.31. Every mutex is malloced
      // (even deprecated GStaticMutex), and actual pthread calls are several
      // layers deep.

      private:
        pthread_mutex_t mutex;

      public:
        void lock()
        {
            if (mt_unlikely (pthread_mutex_lock (&mutex) != 0))
                abort ();
        }

        void unlock ()
        {
            if (mt_unlikely (pthread_mutex_unlock (&mutex) != 0))
                abort ();
        }

        pthread_mutex_t* get_pthread_mutex () { return &mutex; }

        Mutex ()
        {
            if (mt_unlikely (pthread_mutex_init (&mutex, NULL /* mutexattr */) != 0))
                abort ();
        }

        ~Mutex ()
        {
            if (mt_unlikely (pthread_mutex_destroy (&mutex) != 0))
                abort ();
        }
    #elif defined (LIBMARY__OLD_GTHREAD_API)
      private:
        GStaticMutex mutex;
      public:
        /*m Locks the mutex. */
        void lock () { g_static_mutex_lock (&mutex); }
        /*m Unlocks the mutex. */
        void unlock () { g_static_mutex_unlock (&mutex); }
        /* For internal use only: should not be expected to be present in future versions. */
        GMutex* get_glib_mutex () { return g_static_mutex_get_mutex (&mutex); }
         Mutex () { g_static_mutex_init (&mutex); }
        ~Mutex () { g_static_mutex_free (&mutex); }
    #else
      private:
        GMutex mutex;
      public:
        void lock ()   { g_mutex_lock   (&mutex); }
        void unlock () { g_mutex_unlock (&mutex); }
        GMutex* get_glib_mutex () { return &mutex; }
         Mutex () { g_mutex_init  (&mutex); }
        ~Mutex () { g_mutex_clear (&mutex); }
    #endif
  #else
    public:
      void lock   () {}
      void unlock () {}

      Mutex () {}
  #endif
};

}


#endif /* LIBMARY__MUTEX__H__ */

