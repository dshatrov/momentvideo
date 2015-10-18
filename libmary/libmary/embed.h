/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__EMBED__H__
#define LIBMARY__EMBED__H__


#include <libmary/types_base.h>
#include <libmary/std_atomic.h>

#include <utility>


namespace M {

class EmbedContainer
{
    LIBMARY_NO_COPY (EmbedContainer)

  private:
    // Data which lives beyond ~EmbedContainer() dtor call.
    class EmbedContainer_Data
    {
      public:
        Atomic<Count> refcnt;

        // This is the result of dynamic_cast<void*>(this) before calling
        // this->~EmbedContainer(). After dtor is called, vptr is changed
        // and dynamic_cast<void*> returns a different value.
        void *delete_ptr;

        EmbedContainer_Data ()
            : refcnt (1),
              delete_ptr (NULL)
        {}
    };

    __attribute__ ((aligned (alignof (EmbedContainer_Data)))) char data_store [sizeof (EmbedContainer_Data)];
    mt_const EmbedContainer *embed_container;

    EmbedContainer* getEmbedContainer () { return embed_container ? embed_container : this; }

    void embedContainer_ref ()
    {
        EmbedContainer_Data * const data = reinterpret_cast <EmbedContainer_Data*> (data_store);

        data->refcnt.fetch_add (1, std::memory_order_relaxed);
    }

    static void embedContainer_unref (EmbedContainer_Data * const data)
    {
      // 'self' is not a valid object: dtor might have been called.

        if (data->refcnt.fetch_sub (1, std::memory_order_release) != 1)
            return;

        // atomic_thread_fence is not available in g++-4.6
        // std::atomic_thread_fence (std::memory_order_acquire);
        data->refcnt.load (std::memory_order_acquire);

        data->~EmbedContainer_Data ();
        operator delete (data->delete_ptr);
    }

  protected:
    void embedContainer_doDelete ()
    {
        EmbedContainer * const embed_container = getEmbedContainer();
        EmbedContainer_Data * const data = reinterpret_cast <EmbedContainer_Data*> (embed_container->data_store);

        if (embed_container == this) {
            // By using static_cast instead of dynamic_cast here, we forbid virtual inheritance
            // from class EmbedContainer.
            //
            // data->delete_ptr = static_cast <void*> (this);

            // We need dynamic_cast to cast from virtual base EmbedContainer* to
            // outer object's pointer (void*).
            //
            data->delete_ptr = dynamic_cast <void*> (this);
        }

        this->~EmbedContainer ();
        embedContainer_unref (data);
    }

    EmbedContainer (EmbedContainer * const embed_container)
    {
        new (data_store) EmbedContainer_Data;

        if (embed_container) {
            this->embed_container = embed_container->getEmbedContainer ();
            this->embed_container->embedContainer_ref ();
        } else {
            this->embed_container = NULL;
        }
    }

  public:
    virtual ~EmbedContainer () {}
};

template <class T>
class Embed
{
  public:
    // alignas is supported in g++-4.8 and higher. We'll switch to that after
    // we stop using g++-4.6.
    // alignas (T) unsigned char data [sizeof (T)];
    __attribute__ ((aligned (alignof (T)))) char data [sizeof (T)];

    T const * operator-> () const { return reinterpret_cast <T const *> (data); }
    T       * operator-> ()       { return reinterpret_cast <T       *> (data); }

    operator T const *   () const { return reinterpret_cast <T const *> (data); }
    operator T       *   ()       { return reinterpret_cast <T       *> (data); }

    T const * ptr        () const { return reinterpret_cast <T const *> (data); }
    T       * ptr        ()       { return reinterpret_cast <T       *> (data); }

    template <typename ...Args>
    Embed (Args && ...args)
    {
        new (reinterpret_cast <T*> (data)) T (std::forward <Args> (args)...);
    }

    ~Embed ()
    {
        T * const ptr = reinterpret_cast <T*> (data);
        ptr->libMary_unref ();
    }
};

}


#endif /* LIBMARY__EMBED__H__ */

