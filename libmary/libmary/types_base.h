/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__TYPES_BASE__H__
#define LIBMARY__TYPES_BASE__H__


#ifdef __GNUC__
  #define mt_likely(x)   (__builtin_expect(!!(x), 1))
  #define mt_unlikely(x) (__builtin_expect(!!(x), 0))
#else
  #define mt_likely(x)   (x)
  #define mt_unlikely(x) (x)
#endif


// gcc 4.8 wants this
#define __STDC_LIMIT_MACROS


#ifdef LIBMARY_XCODE_BUILD
  #define LIBMARY_PTHREAD 1
  #define LIBMARY_MT_SAFE 1
  #define LIBMARY_ENABLE_KQUEUE 1
  #define LIBMARY_PLATFORM_MACOSX 1
#else
  #include <libmary/libmary_config.h>
#endif


#ifdef __APPLE__
  #include <TargetConditionals.h>
#endif

#if !defined LIBMARY_PLATFORM_WIN32 && !(defined __APPLE__ && defined TARGET_OS_IPHONE)
  #define LIBMARY_TLOCAL __thread
#endif


#ifdef LIBMARY_GLIB
  #include <glib.h>
  #include <stdint.h>
#else
  #include <climits>
  // This include file is C++0x-specific.
  #include <cstdint>
#endif

#if 0
// Another approach to getting numeric limits macros. This one suffers from
// the need for libmary/types_base.h to be the first header used
// in a translation unit.

// Macros like INT8_MAX should be explicitly requested to be defined in C++.
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#endif

#include <cstddef>
#include <cassert>

#include <new>

#ifdef LIBMARY_GLIB
  #if !GLIB_CHECK_VERSION (2, 31, 0)
    #define LIBMARY__OLD_GTHREAD_API
  #endif
#endif

#include <libmary/annotations.h>


#define LIBMARY_NO_COPY(T) \
        T (T const &) = delete; \
        T& operator= (T const &) = delete;


namespace M {

#ifdef LIBMARY_PTHREAD
  typedef void* _libMary_VoidPtr;
#else
  typedef gpointer _libMary_VoidPtr;
#endif

typedef int8_t  Int8;
typedef int16_t Int16;
typedef int32_t Int32;
typedef int64_t Int64;

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;

#ifdef LIBMARY_GLIB
  // Glib macros are more convenient to use because they allow not to abuse
  // standard headers.
  enum {
      Int_Max = G_MAXINT,

      Int8_Max  = G_MAXINT8,
      Int8_Min  = G_MININT8,

      Int16_Max = G_MAXINT16,
      Int16_Min = G_MININT16,

      Int32_Max = G_MAXINT32,
      Int32_Min = G_MININT32,

      Int64_Max = G_MAXINT64,
      Int64_Min = G_MININT64,

      FileOffset_Max = G_MAXINT64,
      FileOffset_Min = G_MININT64
  };

  enum {
      Uint8_Max  = G_MAXUINT8,
      Uint16_Max = G_MAXUINT16,
      Uint32_Max = G_MAXUINT32,
      Uint64_Max = G_MAXUINT64,

      Size_Max  = G_MAXSIZE,
      Count_Max = G_MAXSIZE,

      FileSize_Max = G_MAXUINT64
  };
#else
  // Note that C++ <limits> header is useless, because numeric_limits<>::max/min
  // can't be used in constant expressions.
  enum {
      Int_Max   = INT_MAX,

      Int8_Max  = INT8_MAX,
      Int8_Min  = INT8_MIN,

      Int16_Max = INT16_MAX,
      Int16_Min = INT16_MIN,

      Int32_Max = INT32_MAX,
      Int32_Min = INT32_MIN,

      Int64_Max = INT64_MAX,
      Int64_Min = INT64_MIN,

      FileOffset_Max = INT64_MAX,
      FileOffset_Min = INT64_MIN
  };

  enum {
      Uint8_Max  = UINT8_MAX,
      Uint16_Max = UINT16_MAX,
      Uint32_Max = UINT32_MAX,
      Uint64_Max = UINT64_MAX,

      Size_Max  = SIZE_MAX,
      Count_Max = SIZE_MAX,

      FileSize_Max = UINT64_MAX
  };
#endif

typedef Uint8 Byte;

typedef size_t    Size;
// Signed offset. Use 'Size' for unsigned offsets.
typedef ptrdiff_t Offset;
typedef uintptr_t UintPtr;
typedef Size      Count;

typedef Int64  FileOffset;
typedef Uint64 FileSize;

typedef Uint64 Time;

// Auto-initialized boolean for porting MyCpp code.
class Bool
{
protected:
    bool value;
public:
    Bool& operator = (bool value) { this->value = value; return *this; }
    operator bool () const { return value; }
    Bool (bool value) : value (value) {}
    Bool () { value = false; }
};

// This is a replacement for std::align, which is not in libstdc++ yet.
// We have to use implementation-defined pointer conversions here.
static inline void* alignPtr (void   * const ptr,
                              size_t   const type_alignment)
{
    return (char*) ((size_t) ((char*) ptr + (type_alignment - 1)) & -type_alignment);
}

}


#endif /* LIBMARY__TYPES_BASE__H__ */

