/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__OUTPUT_STREAM__H__
#define LIBMARY__OUTPUT_STREAM__H__


#include <libmary/exception.h>


namespace M {

class OutputStream : public virtual Referenced
{
  public:
    // If one ever needs to detect 'Eof' condition on writes, a convention
    // on throwing a particular kind of IoException will be added.
    // I don't expect that will ever be needed, though.
    //
    // @ret_nwritten is valid aftern write() returns in any case.
    // It write() returned Failure, then the value of *ret_nwritten indicates
    // how much data has been written successfully before the error occured.
    // This value may be less than the actual number of bytes written.
    virtual mt_throws Result write (ConstMemory  mem,
                                    Size        *ret_nwritten) = 0;

    virtual mt_throws Result writev (IovArrayElement *iovs,
                                     Count            num_iovs,
                                     Size            *ret_nwritten);

    virtual mt_throws Result flush () = 0;

    virtual ~OutputStream () {}

  // Non-virtual methods

    mt_throws Result writeFull (ConstMemory  mem,
                                Size        *ret_nwritten);

    mt_throws Result writevFull (IovArrayElement *iovs,
                                 Count            num_iovs,
                                 Size            *ret_nwritten);

  private:
    mt_throws Result writevFull_tail (IovArrayElement *iovs,
                                      Count            num_iovs,
                                      Size             total_len,
                                      Size             total_written,
                                      Size            *ret_nwritten);

  public:
    // Array arguments are treated as "char const *" without this workaround.
    template <Size N>
    mt_throws Result print_ (char const (&str) [N])
    {
        return print_<char const [N]> (str);
    }

    // Empty string (consists of a single terminating null byte).
    mt_throws Result print_ (char const (& /* str */) [1])
    {
      // No-op
        return Result::Success;
    }

    template <class T>
    mt_throws Result print_ (T const &val)
    {
        return do_print_ (val, fmt_def);
    }

    template <class T>
    mt_throws Result print_ (T const &val,
                             Format const &fmt)
    {
        return do_print_ (val, fmt);
    }

    Result print (Format const & /* fmt */)
    {
      // No-op
        return Result::Success;
    }

    template <class T, class ...Args>
    mt_throws Result print (Format const &fmt,
                            T      const &val,
                            Args   const &...args)
    {
        if (!do_print_ (val, fmt))
            return Result::Failure;

        return print (fmt, args...);
    }

    template <class ...Args>
    mt_throws Result print (Format const & /* fmt */,
                            Format const &new_fmt,
                            Args   const &...args)
    {
        return print (new_fmt, args...);
    }

    template <class ...Args>
    mt_throws Result print (Args const &...args)
    {
        return print (fmt_def, args...);
    }

    template <class ...Args>
    mt_throws Result println (Args const &...args)
    {
        if (!print (args..., "\n"))
            return Result::Failure;

        if (!flush ())
            return Result::Failure;

        return Result::Success;
    }

  private:
    template <Size N>
    mt_throws Result do_print_ (char const (&str) [N],
                                Format const & /* fmt */)
    {
        return writeFull (ConstMemory (str, N -  1), NULL /* nwritten */);
    }

    mt_throws Result do_print_ (char const (& /* str */) [1],
                                Format const & /* fmt */)
    {
      // No-op
        return Result::Success;
    }

    mt_throws Result do_print_ (char * const str,
                                Format const & /* fmt */)
    {
        if (str)
            return writeFull (ConstMemory (str, strlen (str)), NULL /* nwritten */);

        return Result::Success;
    }

    mt_throws Result do_print_ (char const *str,
                                Format const & /* fmt */)
    {
        if (str)
            return writeFull (ConstMemory (str, strlen (str)), NULL /* nwritten */);

        return Result::Success;
    }

    mt_throws Result do_print_ (Memory const &mem,
                                Format const & /* fmt */)
    {
        return writeFull (mem, NULL /* nwritten */);
    }

    mt_throws Result do_print_ (ConstMemory const &mem,
                                Format const & /* fmt */)
    {
        return writeFull (mem, NULL /* nwritten */);
    }

    mt_throws Result do_print_ (String * const mt_nonnull str,
                                Format const & /* fmt */)
    {
        if (str)
            return writeFull (str->mem(), NULL /* nwritten */);

        return Result::Success;
    }

    mt_throws Result do_print_ (StRef<String> const mt_nonnull &str,
                                Format const & /* fmt */)
    {
        if (str)
            return writeFull (str->mem(), NULL /* nwritten */);

        return Result::Success;
    }

    // Everything that cannot be easily treated as a byte array needs a call
    // to toString() and goes here.
    template <class C>
    mt_throws Result do_print_ (C      const &val,
                                Format const &fmt)
    {
        char buf [1024];
        Size const len = toString (Memory::forObject (buf), val, fmt);
        if (len <= sizeof (buf)) {
          // The string fits into the buffer on the stack. Writing it out.
            return writeFull (ConstMemory (buf, len), NULL /* nwritten */);
        } else {
          // The string doesn't fit. Using the heap for temporal storage.
            return writeFull (toString (val, fmt)->mem(), NULL /* nwritten */);
        }
    }
};

}


#endif /* LIBMARY__OUTPUT_STREAM__H__ */

