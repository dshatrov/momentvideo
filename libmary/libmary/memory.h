/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__MEMORY__H__
#define LIBMARY__MEMORY__H__


// No libmary #include directives here.
// This file is meant to be included via libmary/types.h
#if !(defined (LIBMARY__TYPES__H__)  || \
      defined (LIBMARY__STRING__H__) || \
      defined (LIBMARY__UTIL_BASE__H__))
#error libmary/memory.h should not be included directly
#endif

namespace M {

class Memory
{
  private:
    Byte *mem_;
    Size len_;

  public:
    // Deprecated
    Byte* mem () const { return mem_; }

    Byte* buf () const { return mem_; }
    Size  len () const { return len_; }

    // TODO rename to isEmpty(). isNull() is for "mem_ == NULL" test.
    bool isNull () const { return len_ == 0; }

    bool isEmpty () const { return len_ == 0; }

    Memory region (Size const region_start,
		   Size const region_len) const
    {
	assert (region_start <= len_);
	assert (len_ - region_start >= region_len);
	return Memory (mem_ + region_start, region_len);
    }

    Memory safeRegion (Size const region_start,
		       Size const region_len) const
    {
	if (region_start >= len_)
	    return Memory ();

	if (region_start + region_len > len_)
	    return Memory (mem_ + region_start, len_ - region_start);

	return Memory (mem_ + region_start, region_len);
    }

    Memory region (Size const region_start) const
    {
	assert (region_start <= len_);
	return Memory (mem_ + region_start, len_ - region_start);
    }

    Memory safeRegion (Size const region_start) const
    {
	if (region_start >= len_)
	    return Memory ();

	return Memory (mem_ + region_start, len_ - region_start);
    }

    Memory (Byte * const mem,
	    Size const len)
	: mem_ (mem),
	  len_ (len)
    {}

    Memory (char * const mem,
	    Size const len)
	: mem_ (reinterpret_cast <Byte*> (mem)),
	  len_ (len)
    {}

    template <class T>
    static Memory forObject (T &obj)
        { return Memory (reinterpret_cast <Byte*> (&obj), sizeof (obj)); }

    Memory ()
	: mem_ (NULL),
	  len_ (0)
    {}
};

class ConstMemory
{
private:
    Byte const *mem_;
    Size len_;

public:
    // Deprecated
    Byte const * mem () const { return mem_; }

    Byte const * buf () const { return mem_; }
    Size len () const { return len_; }

    // TODO rename to isEmpty(). isNull() is for "mem_ == 0" test.
    bool isNull () const { return len_ == 0; }

    bool isEmpty () const { return len_ == 0; }

    ConstMemory region (Size const region_start,
			Size const region_len) const
    {
	assert (region_start <= len_);
	assert (len_ - region_start >= region_len);
	return ConstMemory (mem_ + region_start, region_len);
    }

    ConstMemory safeRegion (Size const region_start,
			    Size const region_len) const
    {
	if (region_start >= len_)
	    return ConstMemory ();

	if (region_start + region_len > len_)
	    return ConstMemory (mem_ + region_start, len_ - region_start);

	return ConstMemory (mem_ + region_start, region_len);
    }

    ConstMemory region (Size const region_start) const
    {
	assert (region_start <= len_);
	return ConstMemory (mem_ + region_start, len_ - region_start);
    }

    ConstMemory safeRegion (Size const region_start) const
    {
	if (region_start >= len_)
	    return ConstMemory ();

	return ConstMemory (mem_ + region_start, len_ - region_start);
    }

    ConstMemory& operator = (Memory const &memory)
    {
	mem_ = memory.mem ();
	len_ = memory.len ();
	return *this;
    }

    ConstMemory (Memory const &memory)
	: mem_ (memory.mem ()),
	  len_ (memory.len ())
    {}

    ConstMemory (Byte const * const mem,
                 Size const  len)
	: mem_ (mem),
	  len_ (len)
    {}

    ConstMemory (char const * const mem,
                 Size const  len)
	: mem_ (reinterpret_cast <Byte const *> (mem)),
	  len_ (len)
    {}

    template <class T>
    static ConstMemory forObject (T &obj)
        { return ConstMemory (reinterpret_cast <Byte const *> (&obj), sizeof (obj)); }

    // TODO Questionable.
    template <Size N>
    ConstMemory (char const (&str) [N])
	: mem_ (reinterpret_cast <Byte const *> (str)),
	  len_ (sizeof (str) - 1)
    {}

    ConstMemory ()
	: mem_ (NULL),
	  len_ (0)
    {}
};

}


#endif /* LIBMARY__MEMORY__H__ */

