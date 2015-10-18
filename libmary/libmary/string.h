/*  Copyright (C) 2011-2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__STRING__H__
#define LIBMARY__STRING__H__


#include <libmary/types_base.h>
#include <cstring>

#include <libmary/st_referenced.h>
#include <libmary/memory.h>


namespace M {

class String : public StReferenced
{
  private:
    Byte *data_buf;
    // String length not including the zero byte at the end.
    Size  data_len;

  private:
    static Byte no_data [1];

  public:
    Memory            mem () const { return Memory      (data_buf, data_len); }
    ConstMemory const_mem () const { return ConstMemory (data_buf, data_len); }

    ConstMemory _libMary_String_constMem () const { return ConstMemory (data_buf, data_len); }

    // mem() plus space for terminating null byte for makeStringInto().
    Memory cstrMem () const { return Memory (data_buf, data_len + 1); }

    Byte* buf () const { return data_buf; }
    Size  len () const { return data_len; }

    char* cstr () const { return (char*) data_buf; }

    void set (ConstMemory const mem)
    {
        if (data_buf != no_data)
            delete[] data_buf;

        if (mem.len() != 0) {
            data_buf = new (std::nothrow) Byte [mem.len() + 1];
            assert (data_buf);
            memcpy (data_buf, mem.mem(), mem.len());
            data_buf [mem.len()] = 0;
            data_len = mem.len ();
        } else {
            data_buf = no_data;
            data_len = 0;
        }
    }

    // Use this carefully.
    void setLength (Size const len) { data_len = len; }

    // Deprecated in favor of isNullString()
    bool isNull () const { return data_buf == no_data; }

    bool isNullString () const { return data_buf == no_data; }

    static bool isNullString (String const * const str)
        { return !str || str->isNullString(); }

    static Memory      mem       (String const * const str) { return str ? str->mem()       : Memory(); }
    static ConstMemory const_mem (String const * const str) { return str ? str->const_mem() : ConstMemory(); }
    static Byte*       buf       (String const * const str) { return str ? str->buf()       : NULL; }
    static Size        len       (String const * const str) { return str ? str->len()       : 0; }

    // Allocates an additional byte for the trailing zero and sets it to 0.
    void allocate (Size const nbytes)
    {
        if (data_buf != no_data)
            delete[] data_buf;

        if (nbytes > 0) {
            data_buf = new (std::nothrow) Byte [nbytes + 1];
            assert (data_buf);
            data_buf [nbytes] = 0;
            data_len = nbytes;
        } else {
            data_buf = no_data;
            data_len = 0;
        }
    }

    operator Memory      () const { return mem(); }
    operator ConstMemory () const { return const_mem(); }

    String& operator = (String const &str)
    {
        if (data_buf != no_data)
            delete[] data_buf;

        if (str.data_len > 0) {
            data_buf = new (std::nothrow) Byte [str.data_len + 1];
            assert (data_buf);
            memcpy (data_buf, str.data_buf, str.data_len + 1);
            data_len = str.data_len;
        } else {
            data_buf = no_data;
            data_len = 0;
        }

        return *this;
    }

    String& operator = (String &&str)
    {
        if (data_buf != no_data)
            delete[] data_buf;

        data_buf = str.data_buf;
        data_len = str.data_len;

        str.data_buf = no_data;
        str.data_len = 0;

        return *this;
    }

    String (String &&str)
        : data_buf (str.data_buf),
          data_len (str.data_len)
    {
        str.data_buf = no_data;
        str.data_len = 0;
    }

    template <Size N>
    String (char const (&str) [N])
    {
        if (N > 1) {
            data_buf = new (std::nothrow) Byte [N];
            assert (data_buf);
            memcpy (data_buf, str, N);
            data_len = N - 1;
        } else {
            data_buf = no_data;
            data_len = 0;
        }
    }

    String (char const * const str)
    {
        if (str != NULL) {
            Size const len = strlen (str);
            data_buf = new (std::nothrow) Byte [len + 1];
            assert (data_buf);
            memcpy (data_buf, str, len + 1);
            data_len = len;
        } else {
            data_buf = no_data;
            data_len = 0;
        }
    }

    String (ConstMemory const mem)
    {
        if (mem.len() != 0) {
            data_buf = new (std::nothrow) Byte [mem.len() + 1];
            assert (data_buf);
            memcpy (data_buf, mem.mem(), mem.len());
            data_buf [mem.len()] = 0;
            data_len = mem.len ();
        } else {
            data_buf = no_data;
            data_len = 0;
        }
    }

    // Preallocates (nbytes + 1) bytes and sets the last byte to 0.
    String (Size const nbytes)
    {
        if (nbytes > 0) {
            data_buf = new (std::nothrow) Byte [nbytes + 1];
            assert (data_buf);
            data_buf [nbytes] = 0;
            data_len = nbytes;
        } else {
            data_buf = no_data;
            data_len = 0;
        }
    }

    String ()
        : data_buf (no_data),
          data_len (0)
    {}

    ~String ()
    {
        if (data_buf != no_data)
            delete[] data_buf;
    }
};

}


#endif /* LIBMARY__STRING__H__ */

