/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__NAL_READER__H__
#define MOMENT__NAL_READER__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

mt_unsafe class NalReader
{
  private:
    Array * const array;
    Size    const size;
    Size    const offset;

    // number of emulation prevention bytes
    unsigned num_epb;
    Size     pos;
    unsigned bits_in_cache;
    Byte     first_byte;
    Uint64   cache;

    Result doRead (unsigned const nbits)
    {
        if (mt_unlikely (size - pos < (nbits - bits_in_cache + 7) / 8))
            return Result::Failure;

        while (bits_in_cache < nbits) {
            Byte byte;
            {
                bool check_three_byte = true;
                for (;;) {
                    if (mt_unlikely (pos >= size))
                        return Result::Failure;

                    array->get (pos + offset, Memory::forObject (byte));
                    ++pos;

                    if (   check_three_byte
                        && byte           == 3
                        && first_byte     == 0
                        && (cache & 0xff) == 0)
                    {
                        check_three_byte = false;
                        ++num_epb;
                        continue;
                    }

                    break;
                }
            }

            cache = (cache << 8) | first_byte;
            first_byte = byte;
            bits_in_cache += 8;
        }

        return Result::Success;
    }

  public:
    Result skip (unsigned const nbits)
    {
        if (mt_unlikely (!doRead (nbits)))
            return Result::Failure;

        bits_in_cache -= nbits;
        return Result::Success;
    }

    Result skipToByte ()
    {
        if (bits_in_cache == 0) {
            if (mt_likely (size > pos))
                ++pos;
            else
                return Result::Failure;
        }

        bits_in_cache = 0;
        return Result::Success;
    }

    template <typename T>
    Result readUint (T        * const mt_nonnull ret_val,
                     unsigned   const nbits)
    {
        unsigned shift;

        if (!doRead (nbits))
            return Result::Failure;

        shift = bits_in_cache - nbits;
        *ret_val = first_byte >> shift;

        *ret_val |= cache << (8 - shift);
        if (nbits < sizeof (T) * 8)
            *ret_val &= ((T) 1 << nbits) - 1;

        bits_in_cache = shift;

        return Result::Success;
    }

    Result readUe (Uint32 * const mt_nonnull ret_val)
    {
        unsigned i = 0;
        Byte     bit;
        Uint32   val;

        if (mt_unlikely (!readUint (&bit, 1)))
            return Result::Failure;

        while (bit == 0) {
            ++i;
            if (mt_unlikely (!readUint (&bit, 1)))
                return Result::Failure;
        }

        if (mt_unlikely (i > 32))
            return Result::Failure;

        if (mt_unlikely (!readUint (&val, i)))
            return Result::Failure;

        *ret_val = (1 << i) - 1 + val;

        return Result::Success;
    }

    Result readSe (Int32 * const mt_nonnull ret_val)
    {
        Uint32 val;

        if (mt_unlikely (!readUe (&val)))
            return Result::Failure;

        if (val % 2)
            *ret_val = (val / 2) + 1;
        else
            *ret_val = -(val / 2);

        return Result::Success;
    }

    NalReader (Array * const array,
               Size    const size,
               Size    const offset)
        : array         (array),
          size          (size),
          offset        (offset),
          num_epb       (0),
          pos           (0),
          bits_in_cache (0),
          first_byte    (0xff),
          cache         (0xff)
    {}
};

}


#endif /* MOMENT__NAL_READER__H__ */

