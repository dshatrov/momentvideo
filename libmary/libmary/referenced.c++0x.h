/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__REFERENCED__H__
#define LIBMARY__REFERENCED__H__


#include <atomic>

#include <libmary/types.h>


namespace M {

class Referenced
{
private:
    std::atomic<Size> refcount;

    Referenced& operator = (Referenced const &);
    Referenced (Referenced const &);

public:
    void ref ()
    {
	refcount.fetch_add (1, std::memory_order_relaxed);
    }

    void unref ()
    {
	if (refcount.fetch_sub (1, std::memory_order_release) != 1)
	    return;

        // TODO Would it be better to use fetch (mo_acquire) here?
	std::atomic_thread_fence (std::memory_order_acquire);
	delete this;
    }

    // For debugging purposes only.
    Count getRefCount () const
    {
	return refcount.load ();
    }

    Referenced ()
	: refcount (1)
    {}

    virtual ~Referenced () {}
};

}


#endif /* LIBMARY__REFERENCED__H__ */

