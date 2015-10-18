/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__COND__H__
#define LIBMARY__COND__H__


// Condition variables can only be sanely used in multithreaded code.
#ifdef LIBMARY_MT_SAFE

#include <libmary/mutex.h>
#include <libmary/state_mutex.h>


namespace M {

class Cond
{
  #ifdef LIBMARY_PTHREAD
    private:
        pthread_cond_t cond;

    public:
        void signal ()
        {
            if (mt_unlikely (pthread_cond_signal (&cond) != 0))
                abort ();
        }

        void broadcastAll ()
        {
            if (mt_unlikely (pthread_cond_broadcast (&cond) != 0))
                abort ();
        }

        void wait (Mutex &mutex)
        {
            if (mt_unlikely (pthread_cond_wait (&cond, mutex.get_pthread_mutex()) != 0))
                abort ();
        }

        void wait (StateMutex &mutex)
        {
            if (mt_unlikely (pthread_cond_wait (&cond, mutex.get_pthread_mutex()) != 0))
                abort ();
        }

        Cond ()
        {
            if (mt_unlikely (pthread_cond_init (&cond, NULL /* cond_attr */) != 0))
                abort ();
        }

        ~Cond ()
        {
            if (mt_unlikely (pthread_cond_destroy (&cond) != 0))
                abort ();
        }
  #elif defined (LIBMARY__OLD_GTHREAD_API)
    private:
        GCond *cond;
    public:
        void signal       () { g_cond_signal    (cond); }
        void broadcastAll () { g_cond_broadcast (cond); }}
        void wait (Mutex      &mutex) { g_cond_wait (cond, mutex.get_glib_mutex()); }
        void wait (StateMutex &mutex) { g_cond_wait (cond, mutex.get_glib_mutex()); }
         Cond () { cond = g_cond_new (); }
        ~Cond () { g_cond_free (cond); }
  #else
    private:
        GCond cond;
    public:
        void signal       () { g_cond_signal    (&cond); }
        void broadcastAll () { g_cond_broadcast (&cond); }
        void wait (Mutex      &mutex) { g_cond_wait (&cond, mutex.get_glib_mutex()); }
        void wait (StateMutex &mutex) { g_cond_wait (&cond, mutex.get_glib_mutex()); }
         Cond () { g_cond_init  (&cond); }
        ~Cond () { g_cond_clear (&cond); }
  #endif
};

}

#endif // LIBMARY_MT_SAFE


#endif /* LIBMARY__COND__H__ */

