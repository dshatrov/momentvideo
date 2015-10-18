/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__STATE_MUTEX__H__
#define LIBMARY__STATE_MUTEX__H__


#include <libmary/mutex.h>


namespace M {

class StateMutex
{
  #ifdef LIBMARY_MT_SAFE
    private:
      Mutex mutex;

    public:
      #ifdef LIBMARY_PTHREAD
        pthread_mutex_t* get_pthread_mutex () { return mutex.get_pthread_mutex(); }
      #else
        /* For internal use only: should not be expected to be present in future versions. */
        GMutex* get_glib_mutex () { return mutex.get_glib_mutex(); }
      #endif

      void lock   ();
      void unlock ();
  #else
    public:
      // Note: To test 'tlocal->state_mutex_counter' in mt-unsafe mode,
      //       StateMutexLock and StateMutexUnlock should be changed
      //       to actually call StateMutex::lock() and StateMutex::unlock().
      void lock   () {}
      void unlock () {}
  #endif
};

}


#endif /* LIBMARY__STATE_MUTEX__H__ */

