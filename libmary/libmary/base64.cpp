/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/log.h>

#include <libmary/base64.h>


namespace M {

static Byte const base64_alphabet [64] = {
    'A', 'B', 'C', 'D',  'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L',  'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T',  'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b',  'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j',  'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r',  's', 't', 'u', 'v',
    'w', 'x', 'y', 'z',  '0', '1', '2', '3',
    '4', '5', '6', '7',  '8', '9', '+', '/'
};

static Byte const base64_decoding_table [256] = {
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e,  0xff, 0xff, 0xff, 0x3f,
    0x34, 0x36, 0x36, 0x37,  0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff,  0xff, 0x00, 0xff, 0xff,

    0xff, 0x00, 0x01, 0x02,  0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a,  0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12,  0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c,  0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24,  0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c,  0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff,  0xff, 0xff, 0xff, 0xff,

    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,

    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff
};

Size base64Encode (ConstMemory   const decoded_mem,
                   Byte        * const mt_nonnull encoded_buf)
{
    Byte const * const decoded_buf = decoded_mem.buf();
    Size         const decoded_len = decoded_mem.len();

    Size encoded_pos = 0;
    for (Size decoded_pos = 0; decoded_pos < decoded_len; ) {
        Uint32 quantum = decoded_buf [decoded_pos] << 24;

        // a    b    c
        // 0001 0011 0111 -> 000 100 110 111

        unsigned nchars = 4;
        if (decoded_len - decoded_pos >= 3) {
            quantum |=   ((Uint32) decoded_buf [decoded_pos + 1] << 16)
                       | ((Uint32) decoded_buf [decoded_pos + 2] << 8);
            decoded_pos += 3;
        } else
        if (decoded_len - decoded_pos == 2) {
            nchars = 3;
            quantum |= ((Uint32) decoded_buf [decoded_pos + 1] << 16);
            decoded_pos += 2;
        } else {
            assert (decoded_len - decoded_pos == 1);
            nchars = 2;
            decoded_pos += 1;
        }

        unsigned i = 0;
        for (; i < nchars; ++i) {
            encoded_buf [encoded_pos] = base64_alphabet [quantum >> 26];
            quantum <<= 6;
            ++encoded_pos;
        }

        for (; i < 4; ++i) {
            encoded_buf [encoded_pos] = '=';
            ++encoded_pos;
        }
    }

    return encoded_pos;
}

Size base64Decode (ConstMemory   const encoded_mem,
                   Byte        * const mt_nonnull decoded_buf)
{
    Byte const * const encoded_buf = encoded_mem.buf();
    Size encoded_len = encoded_mem.len();

    while (encoded_len > 0 && encoded_buf [encoded_len - 1] == '=')
        --encoded_len;

    Size decoded_pos = 0;
    for (Size encoded_pos = 0; encoded_pos < encoded_len; ) {
        Uint32 quantum = base64_decoding_table [encoded_buf [encoded_pos]] << 26;

        unsigned nbytes = 1;
        if (encoded_len - encoded_pos >= 4) {
            quantum |=   ((Uint32) base64_decoding_table [encoded_buf [encoded_pos + 1]] << 20)
                       | ((Uint32) base64_decoding_table [encoded_buf [encoded_pos + 2]] << 14)
                       | ((Uint32) base64_decoding_table [encoded_buf [encoded_pos + 3]] <<  8);
            nbytes = 3;
            encoded_pos += 4;
        } else
        if (encoded_len - encoded_pos == 3) {
            quantum |=   ((Uint32) base64_decoding_table [encoded_buf [encoded_pos + 1]] << 20)
                       | ((Uint32) base64_decoding_table [encoded_buf [encoded_pos + 2]] << 14);
            nbytes = 2;
            encoded_pos += 3;
        } else
        if (encoded_len - encoded_pos == 2) {
            quantum |= (Uint32) base64_decoding_table [encoded_buf [encoded_pos + 1]] << 20;
            nbytes = 1;
            encoded_pos += 2;
        } else {
            assert (encoded_len - encoded_pos == 1);
            nbytes = 1;
            encoded_pos += 1;
        }

        for (unsigned i = 0; i < nbytes; ++i) {
            decoded_buf [decoded_pos] = (Byte) (quantum >> 24);
            quantum <<= 8;
            ++decoded_pos;
        }
    }

    return decoded_pos;
}

}

