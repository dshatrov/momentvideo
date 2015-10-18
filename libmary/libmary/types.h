/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__TYPES__H__
#define LIBMARY__TYPES__H__


#include <libmary/types_base.h>
#include <cstring>

#ifdef LIBMARY_PLATFORM_WIN32
  // winsock2.h has a #warning telling to include it before windows.h
  #include <winsock2.h>
  #include <windows.h>
#else
  #include <sys/uio.h>
  #ifndef __MACH__
    #include <syslimits.h> // for IOV_MAX on FreeBSD
  #endif
#endif

#ifdef LIBMARY_PLATFORM_ANDROID
#include <unistd.h> // for sysconf(_SC_IOV_MAX)
#endif

#include <libmary/std_atomic.h>
#include <libmary/memory.h>


#define LIBMARY__DEFAULT_CREATE_PERMISSIONS 0600


#define _foreach_macro_01(action, x) action (x)
#define _foreach_macro_02(action, x, ...) action (x) _foreach_macro_01 (action, __VA_ARGS__)
#define _foreach_macro_03(action, x, ...) action (x) _foreach_macro_02 (action, __VA_ARGS__)
#define _foreach_macro_04(action, x, ...) action (x) _foreach_macro_03 (action, __VA_ARGS__)
#define _foreach_macro_05(action, x, ...) action (x) _foreach_macro_04 (action, __VA_ARGS__)
#define _foreach_macro_06(action, x, ...) action (x) _foreach_macro_05 (action, __VA_ARGS__)
#define _foreach_macro_07(action, x, ...) action (x) _foreach_macro_06 (action, __VA_ARGS__)
#define _foreach_macro_08(action, x, ...) action (x) _foreach_macro_07 (action, __VA_ARGS__)
#define _foreach_macro_09(action, x, ...) action (x) _foreach_macro_08 (action, __VA_ARGS__)
#define _foreach_macro_10(action, x, ...) action (x) _foreach_macro_09 (action, __VA_ARGS__)
#define _foreach_macro_11(action, x, ...) action (x) _foreach_macro_10 (action, __VA_ARGS__)
#define _foreach_macro_12(action, x, ...) action (x) _foreach_macro_11 (action, __VA_ARGS__)
#define _foreach_macro_13(action, x, ...) action (x) _foreach_macro_12 (action, __VA_ARGS__)
#define _foreach_macro_14(action, x, ...) action (x) _foreach_macro_13 (action, __VA_ARGS__)
#define _foreach_macro_15(action, x, ...) action (x) _foreach_macro_14 (action, __VA_ARGS__)
#define _foreach_macro_16(action, x, ...) action (x) _foreach_macro_15 (action, __VA_ARGS__)
#define _foreach_macro(_01, _02, _03, _04, _05, _06, _07, _08, _09, _10, _11, _12, _13, _14, _15, _16, name, ...) name
#define _foreach(action, ...) _foreach_macro (__VA_ARGS__, \
                                              _foreach_macro_16, \
                                              _foreach_macro_15, \
                                              _foreach_macro_14, \
                                              _foreach_macro_13, \
                                              _foreach_macro_12, \
                                              _foreach_macro_11, \
                                              _foreach_macro_10, \
                                              _foreach_macro_09, \
                                              _foreach_macro_08, \
                                              _foreach_macro_07, \
                                              _foreach_macro_06, \
                                              _foreach_macro_05, \
                                              _foreach_macro_04, \
                                              _foreach_macro_03, \
                                              _foreach_macro_02, \
                                              _foreach_macro_01) \
                                      (action, __VA_ARGS__)


#define unreachable3(line) (::M::_libMary_unreachable_ (__FILE__ ":" #line "::", __func__))
#define unreachable2(line) unreachable3(line)
#define unreachable() unreachable2(__LINE__)

namespace M {
    class ConstMemory;
    void _libMary_unreachable_ (ConstMemory const &mem,
                                ConstMemory const &func);
}


#define enum_beg(T) \
    struct T {

#define _enum_value(value) \
    case value: return toString (mem, #value);

#define enum_values(...)                                                              \
    enum Value {                                                                      \
        __VA_ARGS__                                                                   \
    };                                                                                \
                                                                                      \
    ::M::Size toString_ (::M::Memory const &mem, ::M::Format const & /* fmt */) const \
    {                                                                                 \
        switch (value) {                                                              \
            _foreach (_enum_value, __VA_ARGS__)                                       \
        }                                                                             \
        unreachable ();                                                               \
        return 0;                                                                     \
    }

#define enum_end(T)                                     \
        Value val () const { return value; }            \
        operator Value () const { return value; }       \
        T (Value const value) : value (value) {}        \
        T () {}                                         \
      private:                                          \
        operator bool () const; /* forbidden */         \
        Value value;                                    \
    };


namespace M {

typedef void (*VoidFunction) (void);
typedef void (GenericCallback) (void *cb_data);

class EmptyBase {};

ConstMemory _libMary_stripFuncFilePath (char const *str);

// TODO Move to log.h, append current session id string (log_prefix)

// _func2 and _func3 are a workaround to stringify __LINE__.

#define _func3(line, ...)                                                                            \
        ::M::_libMary_stripFuncFilePath (__FILE__),                                                  \
        ":" #line,                                                                                   \
        ":", __func__, ":",                                                                          \
        ::M::ConstMemory ("                                         " /* 41 spaces */,               \
                     ({ ::M::Size const file_len = ::M::_libMary_stripFuncFilePath (__FILE__).len(); \
                       file_len + sizeof (#line) + sizeof (__func__) + 3 < 40 + 1 ?                  \
                               40 - file_len - sizeof (#line) - sizeof (__func__) - 3 + 1 : 1; }))   \
        __VA_ARGS__

#define _func3_(line, ...)                                                                           \
        ::M::_libMary_stripFuncFilePath (__FILE__),                                                  \
        ":" #line,                                                                                   \
        ":" , __func__,                                                                              \
        ::M::ConstMemory ("                                         " /* 41 spaces */,               \
                     ({ ::M::Size const file_len = ::M::_libMary_stripFuncFilePath (__FILE__).len(); \
                        file_len + sizeof (#line) + sizeof (__func__) + 2 < 40 + 1 ?                 \
                                40 - file_len - sizeof (#line) - sizeof (__func__) - 2 + 1 : 1; }))  \
        __VA_ARGS__

// No line padding  #define _func3(line) __FILE__ ":" #line ":", __func__
#define _func2( line, ...) _func3 (line __VA_ARGS__)
#define _func2_(line, ...) _func3_(line __VA_ARGS__)

#define _func       _func2 (__LINE__)
// TODO explain the extra commas
#define _this_func  _func2 (__LINE__, , , ::M::fmt_hex, "0x", (unsigned long)         this, ::M::fmt_def, " ")
#define _self_func  _func2 (__LINE__, , , ::M::fmt_hex, "0x", (unsigned long) (void*) self, ::M::fmt_def, " ")
#define _func_      _func2_(__LINE__)
#define _this_func_ _func2_(__LINE__, , , ::M::fmt_hex, "0x", (unsigned long)         this, ::M::fmt_def, " ")
#define _self_func_ _func2_(__LINE__, , , ::M::fmt_hex, "0x", (unsigned long) (void*) self, ::M::fmt_def, " ")

template <class T> UintPtr uintPtr (T const &ptr) { return (UintPtr) ptr; };

#define _funcp_action(ptr) #ptr " 0x", ::M::fmt_hex, ::M::uintPtr (ptr),
#define _funcp(...)       _func,       _foreach (_funcp_action, __VA_ARGS__) ::M::fmt_def, " "
#define _self_funcp(...)  _self_func,  _foreach (_funcp_action, __VA_ARGS__) ::M::fmt_def, " "
#define _this_funcp(...)  _this_func,  _foreach (_funcp_action, __VA_ARGS__) ::M::fmt_def, " "
#define _funcp_(...)      _func_,      _foreach (_funcp_action, __VA_ARGS__) ::M::fmt_def
#define _self_funcp_(...) _self_func_, _foreach (_funcp_action, __VA_ARGS__) ::M::fmt_def
#define _this_funcp_(...) _this_func_, _foreach (_funcp_action, __VA_ARGS__) ::M::fmt_def

#define _ptr(ptr) "0x", ::M::fmt_hex, ::M::uintPtr (ptr), ::M::fmt_def
#define _hex(a)   "0x", ::M::fmt_hex,                (a), ::M::fmt_def

class Format;

struct Result
{
    enum Value {
	Failure = 0,
	Success = 1
    };
    operator Value () const { return value; }
    Result (Value const value) : value (value) {}
    Result () {}
    Size toString_ (Memory const &mem, Format const &fmt) const;
  private:
    Value value;
    // conversion to bool is allowed
};

// One should be able to write if (!compare()), whch should mean the same as
// if (compare() == ComparisonResult::Equal).
//
struct ComparisonResult
{
    enum Value {
	Less    = -1,
	Equal   = 0,
	Greater = 1
    };
    operator Value () const { return value; }
    ComparisonResult (Value const value) : value (value) {}
    ComparisonResult () {}
  private:
    operator bool () const; // forbidden
    Value value;
};

#if defined LIBMARY_PLATFORM_ANDROID
    // TODO sysconf() may be too slow
    #define LIBMARY__IOV_MAX ((unsigned) sysconf (_SC_IOV_MAX))
#elif defined LIBMARY_PLATFORM_WIN32
    struct iovec {
        void   *iov_base;
        size_t  iov_len;
    };

    #ifdef IOV_MAX
        #error unexpected: IOV_MAX defined on win32
    #endif

    #define LIBMARY__IOV_MAX (1024)
#else
    // This limit is to ensure that an array of LIBMARY__IOV_MAX IovArrayElement
    // objects may be allocated on stack.
    #define LIBMARY__IOV_MAX__INTERNAL (16384)

    // Note: Comments in boost headers suggest that posix platforms are not
    // required to define IOV_MAX (does that mean that it may be totally missing?)
    #ifdef IOV_MAX
        #if (IOV_MAX) <= LIBMARY__IOV_MAX__INTERNAL
            #define LIBMARY__IOV_MAX (IOV_MAX)
        #else
            #define LIBMARY__IOV_MAX (LIBMARY__IOV_MAX__INTERNAL)
        #endif
    #else
        #define LIBMARY__IOV_MAX (IOV_MAX <= LIBMARY__IOV_MAX__INTERNAL ? IOV_MAX : LIBMARY__IOV_MAX__INTERNAL)
    #endif
#endif

class Object;

}

#include <libmary/virt_ref.h>

namespace M {

template <class T>
class CbDesc
{
  public:
    T const * const cb;
    void    * const cb_data;
    Object  * const guard_obj;

    // Using VirtRef instead of a pointer to avoid loosing reference to 'ref_data'
    // when using CbDesc<> as function's return value. There has been no real
    // problems because of this so far, but this still looks dangerous.
    //    VirtReferenced * const ref_data;
    VirtRef const ref_data;

    T const * operator -> () const { return cb; }

    CbDesc (T const        * const cb,
	    void           * const cb_data,
	    Object         * const guard_obj,
	    VirtReferenced * const ref_data = NULL)
	: cb        (cb),
	  cb_data   (cb_data),
	  guard_obj (guard_obj),
	  ref_data  (ref_data)
    {}

    CbDesc ()
	: cb        (NULL),
	  cb_data   (NULL),
	  guard_obj (NULL),
	  ref_data  (NULL)
    {}
};

#ifdef LIBMARY_WIN32_IOCP
  typedef WSABUF IovArrayElement;
#else
  typedef struct iovec IovArrayElement;
#endif

static inline Memory iovArrayElementMem (IovArrayElement * const elem)
{
  #ifdef LIBMARY_WIN32_IOCP
    return Memory ((Byte*) elem->buf, elem->len);
  #else
    return Memory ((Byte*) elem->iov_base, elem->iov_len);
  #endif
}

static inline Byte* iovArrayElementBuf (IovArrayElement * const elem)
{
  #ifdef LIBMARY_WIN32_IOCP
    return (Byte*) elem->buf;
  #else
    return (Byte*) elem->iov_base;
  #endif
}

static inline Size iovArrayElementLen (IovArrayElement * const elem)
{
  #ifdef LIBMARY_WIN32_IOCP
    return elem->len;
  #else
    return elem->iov_len;
  #endif
}

static inline void iovArrayElementSet (IovArrayElement * const elem,
                                       Byte            * const buf,
                                       Size              const len)
{
  #ifdef LIBMARY_WIN32_IOCP
    elem->buf = (char*) buf;
    elem->len = len;
  #else
    elem->iov_base = buf;
    elem->iov_len  = len;
  #endif
}

struct IovArray
{
    IovArrayElement * const elems;
    Count             const max_num_elems;

  #ifdef LIBMARY_WIN32_IOCP
    Size num_bytes;
  #endif
    Count i;

    Memory getIov (Count const idx)
        { return iovArrayElementMem (&elems [idx]); }

    void addIov (Byte * const buf,
                 Size   const len)
    {
        assert (i < max_num_elems);
      #ifdef LIBMARY_WIN32_IOCP
        elems [i].buf = (char*) buf;
        elems [i].len = len;
        num_bytes += len;
      #else
        elems [i].iov_base = buf;
        elems [i].iov_len  = len;
      #endif
        ++i;
    }

    IovArray (IovArrayElement * const elems,
              Size              const max_num_elems)
        : elems         (elems),
          max_num_elems (max_num_elems),
        #ifdef LIBMARY_WIN32_IOCP
          num_bytes     (0),
        #endif
          i             (0)
    {}
};

}


#if defined __MACH__ || defined (LIBMARY_PLATFORM_WIN32)
static inline void* memrchr (const void *s, int c, size_t n)
{
    for (size_t i = n; i > 0; --i) {
        if (((char*) s) [i - 1] == c)
            return (char*) s + (i - 1);
    }

    return NULL;
}
#endif


#include <libmary/st_referenced.h>


namespace M {

class Buffer : public StReferenced
{
  public:
    Memory mem;

    void allocate (Size const len)
    {
        if (mem.len() != len) {
            delete[] mem.mem();
            Byte * const buf = new (std::nothrow) Byte [len];
            assert (buf);
            mem = Memory (buf, len);
        }
    }

    void setSize (Size const size) { mem = Memory (mem.mem(), size); }

    void set (ConstMemory const from)
    {
        if (from.len()) {
            allocate (from.len());
            memcpy (mem.buf(), from.buf(), from.len());
        } else {
            delete[] mem.mem();
            mem = Memory();
        }
    }

    /*
    Buffer (ConstMemory const from)
    {
        if (from.len()) {
            allocate (from.len());
            memcpy (mem.buf(), from.buf(), from.len());
        }
    }
    */

    Buffer () {}

    ~Buffer () { delete[] mem.mem(); }
};

}


// * For class Format.
// * For toString() (see enum definitions below).
#include <libmary/util_str_base.h>


namespace M {

enum_beg (IoResult)
  enum_values (Normal, Eof, Error)
enum_end (IoResult)

enum_beg (AsyncIoResult)
    enum_values (
        Normal,
      #ifndef LIBMARY_WIN32_IOCP
        // We've got the data and we know for sure that the following call to
        // read() will return EAGAIN.
        Normal_Again,
        // Normal_Eof is usually returned when we've received Hup event for
        // the connection, but there was some data to read.
        Normal_Eof,
      #endif
        Again,
        Eof,
        Error
    )

    bool isAgain () const
    {
      #ifndef LIBMARY_WIN32_IOCP
        return value == Again || value == Normal_Again;
      #else
        return value == Again;
      #endif
    }

    bool isEof () const
    {
      #ifndef LIBMARY_WIN32_IOCP
        return value == Eof || value == Normal_Eof;
      #else
        return value == Eof;
      #endif
    }
enum_end (AsyncIoResult)

enum_beg (SeekOrigin)
    enum_values (Beg, Cur, End)
enum_end (SeekOrigin)

enum_beg (FileAccessMode)
    enum_values (ReadOnly, WriteOnly, ReadWrite)
enum_end (FileAccessMode)

struct FileOpenFlags
{
    enum Value {
        Create   = 0x1,
        Truncate = 0x2,
        Append   = 0x4
    };
    operator Uint32 () const { return value; }
    FileOpenFlags (Value value) : value (value) {}
    FileOpenFlags () {}
  private:
    Value value;
    // conversion to bool is allowed
};

enum_beg (FileType)
    enum_values (
        BlockDevice,
        CharacterDevice,
        Fifo,
        RegularFile,
        Directory,
        SymbolicLink,
        Socket
    )
enum_end (FileType)

class FileStat
{
  public:
    unsigned long long size;
    FileType file_type;
};

}


#endif /* LIBMARY__TYPES__H__ */

