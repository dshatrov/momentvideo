/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__REFERENCED__H__
#define LIBMARY__REFERENCED__H__


#include <libmary/types_base.h>
#ifdef LIBMARY_MT_SAFE
  #include <atomic>
#endif

#include <libmary/virt_referenced.h>
#include <libmary/debug.h>


namespace M {

class Object;

class Referenced : public virtual VirtReferenced
{
    friend class Object;

  private:
    #ifdef LIBMARY_MT_SAFE
      std::atomic<Size> refcnt;
    #else
      Size refcnt;
    #endif

  protected:
    #ifdef LIBMARY_REF_TRACING
      mt_const bool traced;

      void traceRef ();
      void traceUnref ();
    #endif

    virtual void last_unref () { delete this; }

  public:
    #ifdef LIBMARY_REF_TRACING
      mt_const void traceReferences () { traced = true; }
    #endif

    void libMary_ref ()
    {
      #ifdef LIBMARY_MT_SAFE
        refcnt.fetch_add (1, std::memory_order_relaxed);
      #else
        ++refcnt;
      #endif

      #ifdef LIBMARY_REF_TRACING
        if (traced)
            traceRef ();
      #endif
    }

    void libMary_unref ()
    {
      #ifdef LIBMARY_REF_TRACING
        if (traced)
            traceUnref ();
      #endif

      #ifdef LIBMARY_MT_SAFE
        if (refcnt.fetch_sub (1, std::memory_order_release) != 1)
            return;

        // atomic_thread_fence is not available in g++-4.6
        // std::atomic_thread_fence (std::memory_order_acquire);
        refcnt.load (std::memory_order_acquire);
      #else
        if (--refcnt != 0)
            return;
      #endif

        last_unref ();
    }

    void ref   () { libMary_ref   (); }
    void unref () { libMary_unref (); }

    virtual void virt_ref   () { libMary_ref   (); }
    virtual void virt_unref () { libMary_unref (); }

    // For debugging purposes only.
    Count getRefCount () const
    {
      #ifdef LIBMARY_MT_SAFE
        return refcnt.load (std::memory_order_relaxed);
      #else
        return refcnt;
      #endif
    }

    // Copying is allowed for MyCpp::Exception cloning mechanism to work.
    // There's no real reason to forbid copying of Referenced objects,
    // because it can be done transparently and with zero overhead.
    Referenced & operator = (Referenced const &) { /* No-op */ return *this; }

    Referenced (Referenced const &)
        : VirtReferenced (),
          refcnt (1)
        #ifdef LIBMARY_REF_TRACING
          , traced (false)
        #endif
    {}

    Referenced ()
        : refcnt (1)
        #ifdef LIBMARY_REF_TRACING
          , traced (false)
        #endif
    {}

    virtual ~Referenced () {}
};

template <class T>
class Referenced_UnrefAction
{
  public:
    static void act (T * const obj) { static_cast <Referenced*> (obj)->libMary_unref (); }
};

}


#endif /* LIBMARY__REFERENCED__H__ */

