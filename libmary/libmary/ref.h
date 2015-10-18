/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__REF__H__
#define LIBMARY__REF__H__


#include <libmary/types_base.h>
#include <utility>

#include <libmary/util_base.h>


namespace M {

// Note: don't do static_cast <T*> (ref). Implicit casts should do the necessary
// conversions.
//
// Type T should have two methods to be usable with Ref<>:
//     * libMary_ref();
//     * libMary_unref.
//
template <class T>
class Ref
{
    template <class C> friend class Ref;

  private:
    // mutable to allow move semantics for const Ref<> objects
    mutable T *obj;

    Ref& do_ref (T* const ref)
    {
        if (obj == ref)
            return *this;

        T * const old_obj = obj;

        obj = ref;
        if (ref)
            ref->libMary_ref ();

        // Note that unref() may lead to a dtor call, the code for each
        // may require this Ref to be valid. That's why we change Ref's state
        // first, and call unref() last. Ths same applies to all other
        // calls to unref().
        if (old_obj)
            old_obj->libMary_unref ();

        return *this;
    }

  public:
    template <class C>
    operator C* () const { return obj; }
    // This is necessary for implicit conversions (like conversions to bool).
    // "template <class C> operator C* ()" is not sufficient.
    operator T* () const { return obj; }

    T* operator -> () const { return  obj; }
    T& operator *  () const { return *obj; }

    void doSetNoRef (T * const ref)
    {
        // FIXME This is likely wrong: we should unref the object (-1 +0).
        if (obj == ref)
            return;

        T * const old_obj = obj;
        obj = ref;

        if (old_obj)
            old_obj->libMary_unref ();
    }

    template <class C>
    void setNoRef (C * const ref) { doSetNoRef (ref); }
    void setNoRef (T * const ref) { doSetNoRef (ref); }

    void doSetNoUnref (T * const ref)
    {
        if (obj == ref)
            return;

        obj = ref;
        if (ref)
            obj->libMary_ref ();
    }

    template <class C>
    void setNoUnref (C * const ref) { doSetNoUnref (ref); }
    void setNoUnref (T * const ref) { doSetNoUnref (ref); }

    template <class C>
    void setNoRefUnref (C * const ref) { obj = ref; }
    void setNoRefUnref (T * const ref) { obj = ref; }

    template <class C>
    Ref& operator = (Ref<C> const &ref) { return do_ref (ref.obj); }
    // Note that template <class C> Ref& opreator = (Ref<C> const &ref) does not
    // cover default assignment operator.
    Ref& operator = (Ref    const &ref) { return do_ref (ref.obj); }

    template <class C>
    Ref& operator = (Ref<C> const &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                old_obj->libMary_unref ();
        }

        return *this;
    }

    Ref& operator = (Ref const &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                old_obj->libMary_unref ();
        }

        return *this;
    }

    template <class C>
    Ref& operator = (C* const ref) { return do_ref (ref); }
    // This is necessary for the following to work:
    //     Ref<X> ref;
    //     ref = NULL;
    Ref& operator = (T* const ref) { return do_ref (ref); }

    static Ref<T> createNoRef (T* const ref)
    {
        Ref<T> tmp_ref;
        tmp_ref.obj = ref;
        return tmp_ref;
    }

    template <class C>
    Ref (Ref<C> const &ref)
        : obj (ref.obj)
    {
        if (ref.obj)
            ref.obj->libMary_ref ();
    }

    // Note that template <class C> Ref (Ref<C> const &ref) does not cover
    // default copy constructor.
    //
    // We presume that it is impossible to pass a reference to self to a copy
    // constructor.
    Ref (Ref const &ref)
        : obj (ref.obj)
    {
        if (ref.obj)
            ref.obj->libMary_ref ();
    }

    template <class C>
    Ref (Ref<C> const &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    Ref (Ref const &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    template <class C>
    Ref (C* const ref)
        : obj (ref)
    {
        if (ref)
            ref->libMary_ref ();
    }

    // This is necessary for the following to work:
    //     Ref<X> ref (NULL);
    Ref (T * const ref)
        : obj (ref)
    {
        if (ref)
            ref->libMary_ref ();
    }

    Ref () : obj (NULL) {}

    ~Ref ()
    {
        if (obj)
            obj->libMary_unref ();
    }

  // MyCpp compatibility methods.

    T* ptr () const { return obj; }
    T& der () const { assert (obj); return *obj; }

    bool isNull () const { return !obj; }
};

// "Grabbing" is needed because object's reference count is initiallized to '1'
// on object creation, which allows to use references freely in constructors.
template <class T>
Ref<T> grab (T * const obj)
{
    assert_hard (obj);
    return Ref<T>::createNoRef (obj);
}

// TODO Make sure that atomic reference counter increments/decrements are
//      optimized out by the compiler.
template <class T, class ...Args>
Ref<T> grabNew (Args && ...args)
{
    return grab (new (std::nothrow) T (std::forward <Args> (args)...));
}

template <class T, class ...Args>
Ref<T> grabNewObject (Args && ...args)
{
    return grab (new (std::nothrow) T (NULL /* embed_container */, std::forward <Args> (args)...));
}

template <class T> UintPtr uintPtr (Ref<T> const &ref) { return (UintPtr) ref.ptr(); };

}


#endif /* LIBMARY__REF__H__ */

