/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ST_REFERENCED__H__
#define LIBMARY__ST_REFERENCED__H__


#include <libmary/types_base.h>


namespace M {

class StReferenced
{
private:
    unsigned refcount;

public:
    void libMary_st_ref () { ++refcount; }

    void libMary_st_unref ()
    {
        --refcount;
        if (refcount == 0)
            delete this;
    }

    void ref   () { libMary_st_ref   (); }
    void unref () { libMary_st_unref (); }

    // For debugging purposes only.
    unsigned getRefCount () const { return refcount; }

    StReferenced& operator = (StReferenced const &) { /* No-op */ return *this; }

    StReferenced (StReferenced const &) : refcount (1) {}
    StReferenced () : refcount (1) {}

    virtual ~StReferenced () {}
};

template <class T>
class StReferenced_UnrefAction
{
public:
    static void act (T * const obj) { static_cast <StReferenced*> (obj)->libMary_st_unref (); }
};

}


#endif /* LIBMARY__ST_REFERENCED__H__ */

