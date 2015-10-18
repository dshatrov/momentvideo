/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ST_REF__H__
#define LIBMARY__ST_REF__H__


#include <libmary/st_referenced.h>
#include <libmary/string.h>


namespace M {

template <class T>
class StRef
{
    template <class C> friend class StRef;

  private:
    // mutable to allow move semantics for const StRef<> objects
    mutable T *obj;

    void do_ref (T * const ref)
    {
        if (obj == ref)
            return;

        T * const old_obj = obj;

        obj = ref;
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();

        if (old_obj)
            static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
    }

public:
    T* ptr () const { return obj; }

    template <class C>
        operator C* () const { return  obj; }

    operator ConstMemory () const { return (obj ? obj->_libMary_String_constMem() : ConstMemory()); }

       operator T* () const { return  obj; }
    T* operator -> () const { return  obj; }
    T& operator *  () const { return *obj; }

    template <class C>
    void setNoRef (C * const ref)
    {
        if (obj == ref)
            return;

        T * const old_obj = obj;

        obj = ref;

        if (old_obj)
            static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
    }

    template <class C>
    void setNoUnref (C * const ref)
    {
        if (obj == ref)
            return;

        obj = ref;
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();
    }

    template <class C>
    void setNoRefUnref (C * const ref)
    {
        obj = ref;
    }

    template <class C>
    StRef& operator = (StRef<C> const &ref)
    {
        do_ref (ref.obj);
        return *this;
    }

    template <class C>
    StRef& operator = (StRef<C> const &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
        }

        return *this;
    }

    StRef& operator = (StRef const &ref)
    {
        if (this != &ref)
            do_ref (ref.obj);

        return *this;
    }

    StRef& operator = (StRef const &&ref)
    {
        if (obj != ref.obj) {
            T * const old_obj = obj;

            obj = ref.obj;
            ref.obj = NULL;

            if (old_obj)
                static_cast <StReferenced*> (old_obj)->libMary_st_unref ();
        }

        return *this;
    }

    template <class C>
    StRef& operator = (C * const ref)
    {
        do_ref (ref);
        return *this;
    }

    StRef& operator = (T * const ref)
    {
        do_ref (ref);
        return *this;
    };

    static StRef<T> createNoRef (T * const ref)
    {
        StRef<T> tmp_ref;
        tmp_ref.obj = ref;
        return tmp_ref;
    }

    template <class C>
    StRef (StRef<C> const &ref)
        : obj (ref.obj)
    {
        if (ref.obj)
            static_cast <StReferenced*> (ref.obj)->libMary_st_ref ();
    }

    template <class C>
    StRef (StRef<C> const &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    StRef (StRef const &ref)
        : obj (ref.obj)
    {
        if (ref.obj)
            static_cast <StReferenced*> (ref.obj)->libMary_st_ref ();
    }

    StRef (StRef const &&ref)
        : obj (ref.obj)
    {
        ref.obj = NULL;
    }

    template <class C>
    StRef (C * const ref)
        : obj (ref)
    {
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();
    }

    // This is necessary for the following to work:
    //     Ref<X> ref (NULL);
    StRef (T * const ref)
        : obj (ref)
    {
        if (ref)
            static_cast <StReferenced*> (ref)->libMary_st_ref ();
    }

    StRef ()
        : obj (NULL)
    {}

    ~StRef ()
    {
        if (obj)
            static_cast <StReferenced*> (obj)->libMary_st_unref ();
    }
};

template <class T>
StRef<T> st_grab (T * const obj)
{
    assert_hard (obj);
    return StRef<T>::createNoRef (obj);
}

// TODO Make sure that atomic reference counter increments/decrements are
//      optimized out by the compiler.
template <class T, class ...Args>
StRef<T> st_grabNew (Args && ...args)
{
    return st_grab (new (std::nothrow) T (std::forward <Args> (args)...));
}

}


#endif /* LIBMARY__ST_REF__H__ */

