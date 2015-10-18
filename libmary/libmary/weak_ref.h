/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__WEAK_REF__H__
#define LIBMARY__WEAK_REF__H__


//#include <cstdio> // DEBUG


#include <libmary/ref.h>
#include <libmary/object.h>


namespace M {

// Note that WeakRef<Type> declarations should be valid for incomplete types.
template <class T>
class WeakRef
{
private:
    Ref<Object::Shadow> shadow;

    // 'typed_weak_ptr' has been introduced for compatibilty with MyCpp, which
    // uses virtual base classes heavily, like ": public virtual Object".
    //
    // This pointer is valid only when shadow->weak_ptr is non-NULL.
    // It is used to avoid dynamic_cast'ing shadow->weak_ptr, which is of type
    // 'Object*' in order to serve WeakRef<T> with any T.
    T *typed_weak_ptr;

public:
    Ref<T> getRef () const
    {
	if (!shadow)
	    return NULL;

	Object * const obj = Object::_getRef (shadow);
	if (!obj)
	    return NULL;

	return Ref<T>::createNoRef (typed_weak_ptr);
    }

    T* getRefPtr () const
    {
	if (!shadow)
	    return NULL;

	Object * const obj = Object::_getRef (shadow);
	if (!obj)
	    return NULL;

	return typed_weak_ptr;
    }

    T* getUnsafePtr () const { return typed_weak_ptr; }

    Object::Shadow* getShadowPtr () const { return shadow; }

    bool isValid () const { return shadow; }

    WeakRef& operator = (T * const obj)
    {
	if (obj)
	    shadow = obj->getShadow();
	else
	    shadow = NULL;

	typed_weak_ptr = obj;

	return *this;
    }

    WeakRef (T * const obj)
	: typed_weak_ptr (obj)
    {
	if (obj)
	    shadow = obj->getShadow();
    }

    WeakRef ()
        : typed_weak_ptr (NULL)
    {}

#if 0
    // DEBUG
    ~WeakRef ()
    {
	if (shadow)
	    fprintf (stderr, "--- ~WeakRef(): %lu\n", (unsigned long) shadow->getRefCount());
    }
#endif
};

}


#endif /* LIBMARY__WEAK_REF__H__ */

