#ifndef LIBMARY__UTIL_MEM__H__
#define LIBMARY__UTIL_MEM__H__


#include <libmary/types.h>


namespace M {

static inline Uint64 readBe64 (Byte const * const mt_nonnull buf)
{
    return ((Uint64) buf [0] << 56) |
           ((Uint64) buf [1] << 48) |
           ((Uint64) buf [2] << 40) |
           ((Uint64) buf [3] << 32) |
           ((Uint64) buf [4] << 24) |
           ((Uint64) buf [5] << 16) |
           ((Uint64) buf [6] <<  8) |
           ((Uint64) buf [7] <<  0);
}

static inline Uint32 readBe32 (Byte const * const mt_nonnull buf)
{
    return ((Uint32) buf [0] << 24) |
           ((Uint32) buf [1] << 16) |
           ((Uint32) buf [2] <<  8) |
           ((Uint32) buf [3] <<  0);
}

static inline Uint16 readBe16 (Byte const * const mt_nonnull buf)
{
    return ((Uint16) buf [0] << 8) |
           ((Uint16) buf [1] << 0);
}

static inline void writeBe64 (Byte   * const mt_nonnull buf,
                              Uint64   const value)
{
    buf [0] = (Byte) ((value >> 56) & 0xff);
    buf [1] = (Byte) ((value >> 48) & 0xff);
    buf [2] = (Byte) ((value >> 40) & 0xff);
    buf [3] = (Byte) ((value >> 32) & 0xff);
    buf [4] = (Byte) ((value >> 24) & 0xff);
    buf [5] = (Byte) ((value >> 16) & 0xff);
    buf [6] = (Byte) ((value >>  8) & 0xff);
    buf [7] = (Byte) ((value >>  0) & 0xff);
}

static inline void writeBe32 (Byte   * const mt_nonnull buf,
                              Uint32   const value)
{
    buf [0] = (Byte) ((value >> 24) & 0xff);
    buf [1] = (Byte) ((value >> 16) & 0xff);
    buf [2] = (Byte) ((value >>  8) & 0xff);
    buf [3] = (Byte) ((value >>  0) & 0xff);
}

static inline void writeBe16 (Byte   * const mt_nonnull buf,
                              Uint16   const value)
{
    buf [0] = (Byte) ((value >> 8) & 0xff);
    buf [1] = (Byte) ((value >> 0) & 0xff);
}

}


#endif /* LIBMARY__UTIL_MEM__H__ */

