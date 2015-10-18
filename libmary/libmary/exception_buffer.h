/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__EXCEPTION_BUFFER__H__
#define LIBMARY__EXCEPTION_BUFFER__H__


#include <libmary/types.h>
#include <libmary/string.h>
#include <libmary/referenced.h>
#include <libmary/ref.h>
#include <libmary/intrusive_list.h>
#include <libmary/vstack.h>


#define LIBMARY__EXCEPTION_BUFFER_SIZE 4096


namespace M {

class Exception
{
public:
    Exception *cause;
    ConstMemory file;
    ConstMemory func;
    unsigned long line;

    StRef<String> backtrace () const
    {
        StRef<String> const str = st_grab (new (std::nothrow) String);

        Size size = 0;

#define LIBMARY__BACKTRACE_FMT_ e->getName(), " at ", e->file, ":", e->line, ":", e->func, "\n"

        {
            Exception const *e = this;
            while (e) {
                size += measureString (LIBMARY__BACKTRACE_FMT_);
                e = e->cause;
            }
        }

        // 'size' is sure to be > 0.
        str->allocate (size);

        {
            Memory const mem = str->cstrMem();

            Size pos = 0;
            Exception const *e = this;
            while (e) {
                pos += makeStringInto (mem.region (pos), LIBMARY__BACKTRACE_FMT_);
                e = e->cause;
            }
        }

#undef LIBMARY__BACKTRACE_FMT_

        return str;
    }

    virtual StRef<String> toString ()
    {
        return st_grab (new (std::nothrow) String ("Exception"));
    }

    virtual ConstMemory getName () const
    {
        return ConstMemory ("Exception");
    }

    Exception ()
	: cause (NULL)
    {}

    virtual ~Exception () {}
};

class ExceptionBuffer : public Referenced,
                        public IntrusiveListElement<>
{
private:
    struct ExcHeader
    {
        ExcHeader *prv_hdr;
        Exception *cur_exc;
    };

    VStack vstack;

    ExcHeader *last_hdr;

public:
    Exception* getException () const
    {
        if (last_hdr)
            return last_hdr->cur_exc;

        return NULL;
    }

    Exception* throw_ (Size const len,
                       Size const alignment)
    {
        reset ();
        return push (len, alignment);
    }

    // Returns NULL when the buffer is full.
    Exception* push (Size const len,
                     Size const alignment)
    {
        ExcHeader * const hdr = reinterpret_cast <ExcHeader*> (
                vstack.push_malign (sizeof (ExcHeader), alignof (ExcHeader)));

        hdr->prv_hdr = last_hdr;
        last_hdr = hdr;

        Exception * const ptr = reinterpret_cast <Exception*> (
                vstack.push_malign (len, alignment));
        hdr->cur_exc = ptr;

        return ptr;
    }

    void reset ()
    {
        {
            ExcHeader *hdr = last_hdr;
            while (hdr) {
                hdr->cur_exc->~Exception ();
                hdr = hdr->prv_hdr;
            }
        }

        vstack.clear ();
        last_hdr = NULL;
    }

    ExceptionBuffer (Size const alloc_len)
        : vstack (alloc_len, true /* shrinking */),
          last_hdr (NULL)
    {}

    ~ExceptionBuffer ()
    {
        reset ();
    }
};

}


#endif /* LIBMARY__EXCEPTION_BUFFER__H__ */

