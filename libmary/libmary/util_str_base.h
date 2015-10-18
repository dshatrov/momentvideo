/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_STR_BASE__H__
#define LIBMARY__UTIL_STR_BASE__H__


#include <libmary/types.h>

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>

#include <libmary/ref.h>
#include <libmary/st_ref.h>
#include <libmary/string.h>


namespace M {

StRef<String> errnoToString (int errnum);
char const* errnoString (int errnum);

#if defined(LIBMARY_PLATFORM_WIN32) || defined(LIBMARY_PLATFORM_CYGWIN)
StRef<String> win32ErrorToString (DWORD error);
StRef<String> wsaErrorToString   (int error);
#endif

ConstMemory stripWhitespace (ConstMemory mem);

StRef<String> catenateStrings (ConstMemory left,
                               ConstMemory right);


// ________________________________ toString() _________________________________

class Format
{
public:
    // Valid values: 10, 16.
    unsigned num_base;
    // 0 - not set.
    unsigned min_digits;
    // (unsigned) -1 - not set.
    unsigned precision;

    bool hex_memory;

    enum {
	DefaultNumBase   = 10,
	DefaultMinDigits = 0,
	DefaultPrecision = -1
    };

    Format (unsigned const num_base,
	    unsigned const min_digits,
	    unsigned const precision,
            bool     const hex_memory = false)
	: num_base   (num_base),
	  min_digits (min_digits),
	  precision  (precision),
          hex_memory (hex_memory)
    {}

    Format ()
	: num_base   (10),
	  min_digits (0),
	  precision  ((unsigned) -1),
          hex_memory (false)
    {}
};

extern Format fmt_def;
extern Format fmt_hex;
extern Format fmt_hex_mem;

// TODO Get rid of this.
static Format const &libMary_default_format = fmt_def;

namespace {

    struct FormatFlags
    {
	enum Value {
	    WithPrecision = 0x1,
	    WithMinDigits = 0x2
	};
	operator Value () const { return value; }
	FormatFlags (Value const value) : value (value) {}
	FormatFlags () {}
      private:
	Value value;
        // conversion to bool is allowed
    };

}

template <class T>
StRef<String> toString (T const &obj, Format const &fmt = libMary_default_format)
{
    Size const len = toString (Memory(), obj, fmt);
    StRef<String> const str = st_grab (new (std::nothrow) String (len));
    Memory const mem = str->mem();
    // +1 byte for terminating null byte written by snprintf()
    toString (Memory (mem.mem(), mem.len() + 1), obj, fmt);
    return str;
}

template <class T>
inline Size toString (Memory const &mem, T const &obj, Format const &fmt = libMary_default_format)
{
    return obj.toString_ (mem, fmt);
}

template <Size N>
inline Size toString (Memory const &mem, char const (&str) [N], Format const & /* fmt */ = libMary_default_format)
{
    Size const len = N - 1;
    if (len <= mem.len())
	memcpy (mem.mem(), str, len);

    return len;
}

inline Size toString (Memory const &mem, char * const str, Format const & /* fmt */ = libMary_default_format)
{
    if (str == NULL)
        return 0;

    Size const len = strlen (str);
    if (len <= mem.len())
	memcpy (mem.mem(), str, len);

    return len;
}

inline Size toString (Memory const &mem, char const * const str, Format const & /* fmt */ = libMary_default_format)
{
    if (str == NULL)
        return 0;

    Size const len = strlen (str);
    if (len <= mem.len())
	memcpy (mem.mem(), str, len);

    return len;
}

Size toHexString (Memory const mem, ConstMemory const str, Format const &fmt = libMary_default_format);

inline Size toString (Memory const &mem, ConstMemory const str, Format const &fmt = libMary_default_format)
{
    if (fmt.hex_memory)
        return toHexString (mem, str, fmt);

    Size tocopy = str.len();
    if (tocopy > mem.len())
        tocopy = mem.len();

    if (tocopy > 0)
	memcpy (mem.mem(), str.mem(), tocopy);

    return str.len();
}

inline Size toString (Memory const &mem, Memory const str, Format const &fmt = libMary_default_format)
{
    return toString (mem, ConstMemory (str), fmt);
}

// String casts to both Memory and ConstMemory, hence we need this overload
// to avoid conversion ambiguity.
//
inline Size toString (Memory const &mem, String const &str, Format const &fmt = libMary_default_format)
{
    return toString (mem, str.const_mem(), fmt);
}

inline Size toString (Memory const &mem, StRef<String> const &str, Format const & /* fmt */ = libMary_default_format)
{
    if (!str)
        return 0;

    if (str->len() <= mem.len())
        memcpy (mem.mem(), str->mem().mem(), str->len());

    return str->len();
}

// @flags Combination of FormatFlag flags.
template <class T>
inline Size _libMary_snprintf (Memory      const &mem,
			       ConstMemory const &spec_str,
			       T           const value,
			       Format      const &fmt,
			       Uint32      const flags)
{
  // snprintf() is thread-safe according to POSIX.1c

    char format_str [128];
    format_str [0] = '%';
    Size pos = 1;

    if (flags & FormatFlags::WithMinDigits &&
	fmt.min_digits > 0)
    {
	int const res = snprintf (format_str + pos, sizeof (format_str) - pos, ".%u", (unsigned) fmt.min_digits);
	assert (res >= 0);
	assert ((Size) res < sizeof (format_str) - pos);
	pos += res;
    }

    if (flags & FormatFlags::WithPrecision &&
	fmt.precision != (unsigned) -1)
    {
	int const res = snprintf (format_str + pos, sizeof (format_str) - pos, ".%u", (unsigned) fmt.precision);
	assert (res >= 0);
	assert ((Size) res < sizeof (format_str) - pos);
	pos += res;
    }

    assert (sizeof (format_str) - pos > spec_str.len());
    memcpy (format_str + pos, spec_str.mem(), spec_str.len());
    pos += spec_str.len ();

    assert (pos < sizeof (format_str));
    format_str [pos] = 0;

    int const res = snprintf (reinterpret_cast <char*> (mem.mem()), mem.len(), format_str, value);
    assert (res >= 0);

    return res;
}

inline Size toString (Memory const &mem, char value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned char) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, unsigned char value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned char) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "u", (unsigned) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, signed char value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned char) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, short value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) (unsigned short) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, int value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "d", (int) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, long value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "lx", (unsigned long) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "ld", (long) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, long long value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "llx", (unsigned long long) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "lld", (long long) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, unsigned short value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "u", (unsigned) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, unsigned value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "x", (unsigned) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "u", (unsigned) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, unsigned long value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "lx", (unsigned long) value, fmt, FormatFlags::WithMinDigits);

    Size res = _libMary_snprintf (mem, "lu", (unsigned long) value, fmt, FormatFlags::WithMinDigits);
    return res;
}

inline Size toString (Memory const &mem, unsigned long long value, Format const &fmt = libMary_default_format)
{
    if (fmt.num_base == 16)
	return _libMary_snprintf (mem, "llx", (unsigned long long) value, fmt, FormatFlags::WithMinDigits);

    return _libMary_snprintf (mem, "llu", (unsigned long long) value, fmt, FormatFlags::WithMinDigits);
}

inline Size toString (Memory const &mem, float value, Format const &fmt = libMary_default_format)
{
    return _libMary_snprintf (mem, "f", (double) value, fmt, FormatFlags::WithPrecision);
}

inline Size toString (Memory const &mem, double value, Format const &fmt = libMary_default_format)
{
    return _libMary_snprintf (mem, "f", (double) value, fmt, FormatFlags::WithPrecision);
}

inline Size toString (Memory const &mem, long double value, Format const &fmt = libMary_default_format)
{
    return _libMary_snprintf (mem, "Lf", (long double) value, fmt, FormatFlags::WithPrecision);
}

inline Size toString (Memory const &mem, bool value, Format const & /* fmt */ = libMary_default_format)
{
    return value ? toString (mem, "true") : toString (mem, "false");
}


// _______________________________ printToString _______________________________

static inline
Size _do_printToString (Memory const   /* mem */,
                        Format const & /* fmt */)
{
    return 0;
}

template <class T, class ...Args>
Size _do_printToString (Memory const  mem,
                        Format const &fmt,
                        T      const &obj,
                        Args   const &...args)
{
    Size const size = toString (mem, obj, fmt);
    return size + _do_printToString (mem.safeRegion (size), fmt, args...);
}

template <class ...Args>
Size _do_printToString (Memory const  mem,
                        Format const & /* fmt */,
                        Format const &new_fmt,
                        Args   const &...args)
{
    return _do_printToString (mem, new_fmt, args...);
}

template <class ...Args>
Size printToString (Memory const  mem,
                    Args   const &...args)
{
    return _do_printToString (mem, fmt_def, args...);
}


// _______________________________ measureString _______________________________

static inline
Size _do_measureString (Format const & /* fmt */)
{
    return 0;
}

template <class T, class ...Args>
Size _do_measureString (Format const &fmt,
			T      const &obj,
			Args   const &...args)
{
    Size const sub_len = toString (Memory(), obj, fmt);
    return sub_len + _do_measureString (fmt, args...);
}

template <class ...Args>
Size _do_measureString (Format const & /* fmt */,
			Format const &new_fmt,
			Args   const &...args)
{
    return _do_measureString (new_fmt, args...);
}

template <class ...Args>
Size measureString (Args const &...args)
{
    return _do_measureString (fmt_def, args...);
}


// ________________________________ makeString _________________________________

static inline Size
_do_makeString (Memory const & /* mem */,
		Format const & /* fmt */)
{
  // No-op
    return 0;
}

template <class T, class ...Args>
Size _do_makeString (Memory const &mem,
		     Format const &fmt,
		     T      const &obj,
		     Args   const &...args)
{
    Size const len = toString (mem, obj, fmt);
    assert (len <= mem.len());
    _do_makeString (mem.region (len), fmt, args...);
    return len;
}

template <class ...Args>
Size _do_makeString (Memory const &mem,
		     Format const & /* fmt */,
		     Format const &new_fmt,
		     Args   const &...args)
{
    return _do_makeString (mem, new_fmt, args...);
}

// Note: mem should have space for terminating null byte.
template <class ...Args>
Size makeStringInto (Memory const mem,
                     Args const &...args)
{
   return _do_makeString (mem, fmt_def, args...);
}

template <class ...Args>
StRef<String> makeString (Args const &...args)
{
    StRef<String> const str = st_grab (new (std::nothrow) String (measureString (args...)));
    Memory const mem = str->mem();
    // +1 byte for terminating null byte written by snprintf()
    _do_makeString (Memory (mem.mem(), mem.len() + 1), fmt_def, args...);
    return str;
}

template <class ...Args>
String makeStringVal (Args const &...args)
{
    String str (measureString (args...));
    // +1 byte for terminating null byte written by snprintf()
    _do_makeString (Memory (str.buf(), str.len() + 1), fmt_def, args...);
    return str;
}

// _____________________________________________________________________________


static inline StRef<String> newString ()
{
    return st_grab (new (std::nothrow) String);
}

static inline StRef<String> newString (ConstMemory const mem)
{
    return st_grab (new (std::nothrow) String (mem));
}

static inline StRef<String> newString (Size const nbytes)
{
    return st_grab (new (std::nothrow) String (nbytes));
}

template <Size N>
ComparisonResult compare (ConstMemory const &left,
			  char const (&right) [N])
{
    Size const tocompare = left.len() <= (N - 1) ? left.len() : (N - 1);
    int const res = memcmp (left.mem(), right, tocompare);
    if (res == 0)
	return ComparisonResult::Equal;

    if (res > 0)
	return ComparisonResult::Greater;

    return ComparisonResult::Less;
}

ComparisonResult compare (ConstMemory const &left,
			  ConstMemory const &right);

static inline bool equal (ConstMemory const &left,
			  ConstMemory const &right)
{
    if (left.len() != right.len())
	return false;

    return !memcmp (left.mem(), right.mem(), left.len());
}

static inline void toLowercase (Memory const mem)
{
    for (Size i = 0; i < mem.len(); ++i)
        mem.buf() [i] = tolower (mem.buf() [i]);
}

static inline void toUppercase (Memory const mem)
{
    for (Size i = 0; i < mem.len(); ++i)
        mem.buf() [i] = toupper (mem.buf() [i]);
}

bool stringHasPrefix (ConstMemory str,
                      ConstMemory prefix);

bool stringHasSuffix (ConstMemory  str,
                      ConstMemory  suffix,
                      ConstMemory *ret_str);

static inline unsigned long cstrToUlong (char const * const cstr,
                                         int          const base = 10)
{
    return strtoul (cstr, NULL /* endptr */, base);
}

unsigned long strToUlong (ConstMemory mem,
                          int         base = 10);

}


#endif /* LIBMARY__UTIL_STR_BASE__H__ */

