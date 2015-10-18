/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LINEAR_READER__H__
#define LIBMARY__LINEAR_READER__H__


#include <libmary/page_pool.h>


namespace M {

class LinearMsgByteReader
{
  private:
    PagePool::Page *page;
    Size msg_len;
    Size msg_offs;

    void scrollToData ()
    {
        while (page && page->data_len <= msg_offs) {
            msg_offs -= page->data_len;
            page = page->getNextMsgPage();
        }
    }

  public:
    bool getByte (Byte * const mt_nonnull ret_byte)
    {
        if (mt_unlikely (!page))
            return false;

        *ret_byte = page->getData() [msg_offs];
        --msg_len;
        ++msg_offs;

        if (msg_len == 0)
            page = NULL;

        scrollToData ();
        return true;
    }

    LinearMsgByteReader (PagePool::Page * const page,
                         Size             const msg_len,
                         Size             const msg_offs)
        : page     (mt_likely (msg_len) ? page : NULL),
          msg_len  (msg_len),
          msg_offs (msg_offs)
    {
        scrollToData ();
    }
};

template <class ByteReader>
class LinearBitReader
{
  private:
    ByteReader * const byte_reader;

    Byte     cur_byte;
    unsigned cur_offs;

  public:
    bool getBit (unsigned * const mt_nonnull ret_bit)
    {
        if (cur_offs >= 8) {
            if (!byte_reader->getByte (&cur_byte))
                return false;

            cur_offs = 0;
        }

        *ret_bit = (cur_byte >> (7 - cur_offs)) & 0x1;
        ++cur_offs;

        return true;
    }

    LinearBitReader (ByteReader * const mt_nonnull byte_reader)
        : byte_reader (byte_reader),
          cur_offs    (8)
    {}
};

class LinearMsgBitReader
{
  private:
    LinearMsgByteReader byte_reader;
    LinearBitReader<LinearMsgByteReader> bit_reader;

  public:
    bool getBit (unsigned * const mt_nonnull ret_bit)
        { return bit_reader.getBit (ret_bit); }

    LinearMsgBitReader (PagePool::Page * const page,
                        Size             const msg_len,
                        Size             const msg_offs)
        : byte_reader (page, msg_len, msg_offs),
          bit_reader  (&byte_reader)
    {}
};

template <class BitReader>
static inline bool readExpGolombUint (BitReader * const bit_reader,
                                      Uint64    * const ret_number)
{
    unsigned num_zeroes = 0;
    for (;;) {
        unsigned bit;
        if (!bit_reader->getBit (&bit))
            return false;

        if (bit != 0)
            break;

        ++num_zeroes;
        if (num_zeroes > 64)
            return false;
    }

    Uint64 number = 0;
    for (unsigned i = 0; i < num_zeroes; ++i) {
        unsigned bit;
        if (!bit_reader->getBit (&bit))
            return false;

        number = (number << 1) | bit;
    }

    if (num_zeroes == 64 && number != 0)
        return false;

    number += ((Uint64) 1 << num_zeroes) - 1;

    *ret_number = number;
    return true;
}

}


#endif /* LIBMARY__LINEAR_READER__H__ */

