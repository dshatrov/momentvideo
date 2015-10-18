/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ATOMIC__H__
#define LIBMARY__ATOMIC__H__


#include <libmary/types.h>

#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY_GLIB
    #include <glib.h>
  #else
    #include <atomic>
  #endif
#endif


// Note that g_atomic_*_get/set() macros use some static assertion technique
// which doesn't play well with C++'s "this" pointer. This is reproducible
// with clang.


namespace M {

class AtomicInt
{
private:
  #ifdef LIBMARY_MT_SAFE
    #ifdef LIBMARY_GLIB
      gint volatile value;
    #else
      std::atomic_int value;
    #endif
  #else
    int value;
  #endif

public:
    void set (int const value)
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          gint volatile * const ptr = &this->value;
          g_atomic_int_set (ptr, (gint) value);
        #else
          this->value = value;
        #endif
      #else
        this->value = value;
      #endif
    }

    int get () const
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          return (int) g_atomic_int_get (&value);
        #else
          return value;
        #endif
      #else
        return value;
      #endif
    }

    void inc ()
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          g_atomic_int_inc (&value);
        #else
          ++value;
        #endif
      #else
        ++value;
      #endif
    }

    void dec ()
    {
        #ifdef LIBMARY_MT_SAFE
           #ifdef LIBMARY_GLIB
             (void) g_atomic_int_add (&value, (gint) -1);
           #else
             --value;
           #endif
        #else
           --value;
        #endif
    }

    void add (int const a)
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          (void) g_atomic_int_add (&value, (gint) a);
        #else
          value += a;
        #endif
      #else
        value += a;
      #endif
    }

    int fetchAdd (int const a)
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          #ifdef LIBMARY__OLD_GTHREAD_API
            return (int) g_atomic_int_exchange_and_add (&value, a);
          #else
            return (int) g_atomic_int_add (&value, a);
          #endif
        #else
          return value.fetch_add (a);
        #endif
      #else
        int old_value = value;
        value += a;
        return old_value;
      #endif

      #if 0
        // Alternate version.
          for (;;) {
              gint const old = g_atomic_int_get (&value);
              if (g_atomic_int_compare_and_exchange (&value, old, old + a))
                  return old;
          }
          // unreachable
      #endif
    }

    bool compareAndExchange (int       old_value,
                             int const new_value)
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          return (bool) g_atomic_int_compare_and_exchange (&value,
                                                           (gint) old_value,
                                                           (gint) new_value);
        #else
          return value.compare_exchange_strong (old_value, new_value);
        #endif
      #else
        if (value == old_value) {
            value = new_value;
            return true;
        }

        return false;
      #endif
    }

    bool decAndTest ()
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          return (bool) g_atomic_int_dec_and_test (&value);
        #else
          return --value == 0;
        #endif
      #else
        return --value == 0;
      #endif
    }

    AtomicInt (int const value = 0)
      #ifdef LIBMARY_MT_SAFE
        #ifndef LIBMARY_GLIB
        : value (value)
        #endif
      #endif
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          gint volatile * const ptr = &this->value;
          g_atomic_int_set (ptr, (gint) value);
        #endif
      #else
        this->value = value;
      #endif
    }
};

class AtomicPointer
{
private:
  #ifdef LIBMARY_MT_SAFE
    #ifdef LIBMARY_GLIB
      // Mutable, because g_atomic_pointer_get() takes non-const
      // parameter in mingw.
      mutable gpointer volatile value;
    #else
      std::atomic<void*> value;
    #endif
  #else
    void * value;
  #endif

public:
    void set (void * const value)
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          gpointer volatile * const ptr = &this->value;
          g_atomic_pointer_set (ptr, (gpointer) value);
        #else
          this->value = value;
        #endif
      #else
        this->value = value;
      #endif
    }

    void* get () const
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMATY_GLIB
          gpointer volatile * const ptr = &this->value;
          return (void*) g_atomic_pointer_get (ptr);
        #else
          return value;
        #endif
      #else
        return value;
      #endif
    }

    // _nonatomic methods have been added to reuse Object::shadow for
    // deletion queue list link pointer.
    void set_nonatomic (void * const value)
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          this->value = (gpointer) value;
        #else
          this->value.store (value, std::memory_order_relaxed);
        #endif
      #else
        this->value = value;
      #endif
    }

    void* get_nonatomic () const
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          return (void*) value;
        #else
          return value.load (std::memory_order_relaxed);
        #endif
      #else
        return value;
      #endif
    }

    bool compareAndExchange (void *       old_value,
                             void * const new_value)
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          return (bool) g_atomic_pointer_compare_and_exchange (&value,
                                                               (gpointer) old_value,
                                                               (gpointer) new_value);
        #else
          return value.compare_exchange_strong (old_value, new_value);
        #endif
      #else
        if (value == old_value) {
            value = new_value;
            return true;
        }

        return false;
      #endif
    }

    AtomicPointer (void * const value = NULL)
      #ifdef LIBMARY_MT_SAFE
        #ifndef LIBMARY_GLIB
        : value (value)
        #endif
      #endif
    {
      #ifdef LIBMARY_MT_SAFE
        #ifdef LIBMARY_GLIB
          gpointer volatile * const ptr = &this->value;
          g_atomic_pointer_set (ptr, value);
        #endif
      #else
        this->value = value;
      #endif
    }
};

#ifdef LIBMARY_MT_SAFE
  #ifdef LIBMARY_GLIB
    extern volatile gint _libMary_dummy_mb_int;

    static inline void full_memory_barrier ()
    {
        // g_atomic_int_get() acts as a full compiler and hardware memory barrier.
        g_atomic_int_get (&_libMary_dummy_mb_int);
    }
  #else
    static inline void full_memory_barrier ()
    {
        std::atomic_thread_fence (std::memory_order_seq_cst);
    }
  #endif
#else
  static inline void full_memory_barrier () {}
#endif

}


#endif /* LIBMARY__ATOMIC__H__ */

