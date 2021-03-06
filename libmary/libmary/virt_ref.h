/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__VIRT_REF__H__
#define LIBMARY__VIRT_REF__H__


#include <libmary/virt_referenced.h>


namespace M {

// This class is deliberately minimalistic. It is meant to be used in Callback
// only to hold a referenced to an arbitrary object (Referenced or Object).
class VirtRef
{
private:
    VirtReferenced *ref;

public:
    VirtReferenced* ptr () const { return ref; }

    void setNoRef (VirtReferenced * const obj)
    {
        VirtReferenced * const old = ref;
        ref = obj;
        if (old)
            old->virt_unref ();
    }

#if 0
    void setNoUnref (VirtReferenced * const ref)
    {
	this->ref = ref;
	if (ref)
	    ref->virt_ref ();
    }
#endif

    // TODO Unrefing late in all *Ref<> classes is probably a good idea.
    //      ^^^ For sure!
    //      ^^^ 13.01.05 Did some fixes in this direction.
    //
    // TODO This is now equivalent to "= NULL". Get rid of selfUnref().
    void selfUnref ()
    {
        if (this->ref) {
            VirtReferenced * const tmp_ref = this->ref;
            this->ref = NULL;
            tmp_ref->virt_unref ();
        }
    }

    operator VirtReferenced* () const
    {
        return ref;
    }

    VirtRef& operator = (VirtReferenced * const ref)
    {
        if (this->ref == ref)
            return *this;

        VirtReferenced * const old_obj = this->ref;

	this->ref = ref;
	if (ref)
	    ref->virt_ref ();

	if (old_obj)
	    old_obj->virt_unref ();

	return *this;
    }

    VirtRef& operator = (VirtRef const &virt_ref)
    {
	if (this == &virt_ref || ref == virt_ref.ref)
	    return *this;

        VirtReferenced * const old_obj = ref;

	ref = virt_ref.ref;
	if (ref)
	    ref->virt_ref ();

        if (old_obj)
	    old_obj->virt_unref ();

	return *this;
    }

    VirtRef& operator = (VirtRef &&virt_ref)
    {
        if (ref != virt_ref.ref) {
            VirtReferenced * const old_ref = ref;

            ref = virt_ref.ref;
            virt_ref.ref = NULL;

            if (old_ref)
                old_ref->virt_unref ();
        }

        return *this;
    }

    VirtRef (VirtRef const &virt_ref)
        : ref (virt_ref.ref)
    {
	if (ref)
	    ref->virt_ref ();
    }

    VirtRef (VirtRef &&virt_ref)
        : ref (virt_ref.ref)
    {
        virt_ref.ref = NULL;
    }

    VirtRef (VirtReferenced * const ref)
	: ref (ref)
    {
	if (ref)
	    ref->virt_ref ();
    }

    VirtRef ()
	: ref (NULL)
    {}

    ~VirtRef ()
    {
	if (ref)
	    ref->virt_unref ();
    }
};

}


#endif /* LIBMARY__VIRT_REF__H__ */

