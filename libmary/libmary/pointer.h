/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


// Generic representation for reference types from MyCpp.


#ifndef __LIBMARY__POINTER__H__
#define __LIBMARY__POINTER__H__


#include <libmary/util_base.h>


namespace M {

template <class T> class Ref;
template <class T> class StRef;

// Pointers provide a generic way to reference objects in MyCpp.
// T&, T* and Ref<T> can all be represented by the same Pointer<T>.
// This is convinient when writing templates: one can declare one
// method operating with a Pointer<T> instead of three separate methods
// for T&, T* and Ref<T> (plus necessary cv-variations).
//
// A pointer can be created using compact syntax "pointer(obj)".
// ConstPointer<T> and const_pointer(obj) should be used for pointers
// to const objects.

template <class T>
class Pointer
{
  protected:
    T *ptr;

  public:
    bool isNull () const { return ptr == NULL; }

    operator T* () const { return ptr; }

    T& operator * () const
    {
	assert (ptr);
	return *ptr;
    }

    T* operator -> () const
    {
	return ptr;
    }

    Pointer& operator = (T &obj)
    {
	this->ptr = &obj;
	return *this;
    }

    // Both "T*" and "T const *" land here. The latter fails because
    // 'this->ptr' if of type "T*".
    Pointer& operator = (T *ptr)
    {
	this->ptr = ptr;
	return *this;
    }

  // We must declare both "Ref<T> &" and "Ref<T> const &" versions to avoid
  // overlapping with "T &obj", where "T" is "Ref<T>" or "Ref<T> const".
  // Same applies to constructors.

    Pointer& operator = (Ref<T> &ref)
    {
	this->ptr = ref;
	return *this;
    }

    Pointer& operator = (Ref<T> const &ref)
    {
	this->ptr = ref;
	return *this;
    }

    Pointer& operator = (StRef<T> &ref)
    {
        this->ptr = ref;
        return *this;
    }

    Pointer& operator = (StRef<T> const &ref)
    {
        this->ptr = ref;
        return *this;
    }

    Pointer (T &obj) : ptr (&obj) {}

    // Both "T*" and "T const *" land here. The latter fails because
    // 'this->ptr' is of type "T*".
    Pointer (T *ptr) : ptr (ptr) {}

    Pointer (  Ref<T>       &ref) : ptr (ref) {}
    Pointer (  Ref<T> const &ref) : ptr (ref) {}
    Pointer (StRef<T>       &ref) : ptr (ref) {}
    Pointer (StRef<T> const &ref) : ptr (ref) {}

    Pointer () : ptr (NULL) {}
};

#if 0
// Unused

template <class T>
class ConstPointer
{
protected:
    T const *ptr;

public:
    bool isNull () const
    {
	return ptr == NULL;
    }

    operator T const * () const
    {
	return ptr;
    }

    T const & operator * () const
    {
	assert (ptr);
	return *ptr;
    }

    T const * operator -> () const
    {
	return ptr;
    }

    ConstPointer& operator = (T &obj)
    {
	this->ptr = &obj;
	return *this;
    }

    // Both "T*" and "T const *" land here.
    ConstPointer& operator = (T *ptr)
    {
	this->ptr = ptr;
	return *this;
    }

  // We must declare both "Ref<T> &" and "Ref<T> const &" versions to avoid
  // overlapping with "T &obj", where "T" is "Ref<T>" or "Ref<T> const".
  // Same applies to constructors.

    ConstPointer& operator = (Ref<T> &ref)
    {
	this->ptr = ref;
    }

    ConstPointer& operator = (Ref<T> const &ref)
    {
	this->ptr = ref;
    }

    ConstPointer& operator = (Pointer<T> &p)
    {
	this->ptr = p;
	return *this;
    }

    ConstPointer& operator = (Pointer<T> const &p)
    {
	this->ptr = p;
	return *this;
    }

    ConstPointer (T &obj)
    {
	this->ptr = &obj;
    }

    // Both "T*" and "T const *" land here.
    ConstPointer (T *ptr)
    {
	this->ptr = ptr;
    }

    ConstPointer (Ref<T> &ref)
    {
	this->ptr = ref;
    }

    ConstPointer (Ref<T> const &ref)
    {
	this->ptr = ref;
    }

    ConstPointer (Pointer<T> &p)
    {
	this->ptr = p;
    }

    ConstPointer (Pointer<T> const &p)
    {
	this->ptr = p;
    }

    ConstPointer ()
    {
	this->ptr = NULL;
    }
};
#endif

#if 0
// Unused

template <class T>
Pointer<T> pointer (T &obj)
{
    return Pointer<T> (&obj);
}

template <class T>
Pointer<T> pointer (T *ptr)
{
    return Pointer<T> (ptr);
}

template <class T>
Pointer<T> pointer (Ref<T> &ref)
{
    return Pointer<T> (ref);
}

template <class T>
Pointer<T> pointer (Ref<T> const &ref)
{
    return Pointer<T> (ref);
}

template <class T>
ConstPointer<T> const_pointer (T &ptr)
{
    return ConstPointer<T> (ptr);
}

template <class T>
ConstPointer<T> const_pointer (T *ptr)
{
    return ConstPointer<T> (ptr);
}

template <class T>
ConstPointer<T> const_pointer (Ref<T> &ref)
{
    return ConstPointer<T> (ref.ptr ());
}

template <class T>
ConstPointer<T> const_pointer (Ref<T> const &ref)
{
    return ConstPointer<T> (ref.ptr ());
}
#endif

}


#endif /* __LIBMARY__POINTER__H__ */

