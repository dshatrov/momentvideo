/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/inc.h>

#include <nettle/rsa.h>


using namespace M;

namespace Moment {

/* state_flag: 1 - key present
 *             2 - incorrect system date
 *             4 - key expired
 *
 * features:   bit 1 set - commercial license
 *             equal to 2 - evaluation license
 *             equal to 4 - rtsp+rtmp license
 *             equal to 6 - rtsp license
 *
 * Moment::desc, Moment::state, Moment::trylock - same value (MomentDesc)
 */

//extern StRef<String> primak;

StRef<String> filename_;

static Ref<MConfig::Config> config_;

static Byte const charmap [32] = {
    '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
    'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M',
    'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z'
};

static char const str___ [] = "18c9d5c46c";

static bool parseConfigEntry (ConstMemory   const key_mem,
                              MomentDesc  * const mt_nonnull ret_desc)
{
    if (key_mem.len() != 25 + 4)
        return false;

    char const str_   [] = "1856C0F2720";

    mpz_t key;
    mpz_init (key);

    unsigned pos = 25 + 4  - 1;
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            unsigned k = 0;
            for (k = 0; k < sizeof (charmap); ++k) {
                if (key_mem.mem() [pos] == charmap [k])
                    break;
            }
            if (k == sizeof (charmap)) {
//                logD_ (_func, "false");
                return false;
            }

            mpz_add_ui (key, key, k);
            if (!(i == 4 && j == 4))
                mpz_mul_2exp (key, key, 5);

            --pos;
        }

        --pos;
    }

#if 0
    {
        char * const key_str = mpz_get_str (NULL /* str */, 16 /* base */, key);
        logD_ (_func, "MANGLED HEX: ", key_str);
    }
#endif

    Uint32 dword [4];
    for (int i = 0; i < 4; ++i) {
        dword [i] = (Uint32) mpz_get_ui (key);
        mpz_tdiv_q_2exp (key, key, 32);
    }

    for (int i = 0; i < 3; ++i) {
        dword [3 - i] ^= dword [3 - i - 1];
    }
    dword [3] &= 0x1fffffff;

    char const str__  [] = "FE873B29C21";

    mpz_set_ui (key, 0);
    for (int i = 0; i < 4; ++i) {
        mpz_add_ui (key, key, dword [3 - i]);
        if (i != 3)
            mpz_mul_2exp (key, key, 32);
    }

#if 0
    {
        char * const key_str = mpz_get_str (NULL /* str */, 16 /* base */, key);
        logD_ (_func, "HEX: ", key_str);
    }
#endif

    StRef<String> const n_str = makeString (str___, str_, str__);

//    char const d_str [] = "0BF32A72C738B5C794716A65C1E20505";
//  char e_str [] = "3DF779CD";
    char e_str [] = "4eg889de";

    mpz_t n, /* d, */ e;
    mpz_init_set_str (n, n_str->cstr(), 16);
//    mpz_init_set_str (d, d_str, 16);

    for (unsigned i = 0; i < sizeof (e_str) - 1; ++i) {
        if (e_str [i] != '9')
            --e_str [i];
    }

    mpz_init_set_str (e, e_str, 16);

    mpz_t desc;
    mpz_init (desc);

    mpz_powm (desc, key, e, n);

#if 0
    {
        char * const desc_str = mpz_get_str (NULL /* str */, 16 /* base */, desc);
        logD_ (_func, "DESC: ", desc_str);
    }
#endif

#if 0
    StRef<String> const desc_str = makeString ("c4",
                                               fmt16, date,
                                               "b7",
                                               fmt32, features,
                                               "e39a",
                                               fmt32, number,
                                               "5d");
#endif

    {
        unsigned long const c = mpz_get_ui (desc) & 0xff;
        mpz_tdiv_q_2exp (desc, desc, 8);
        if (c != 0x5d) {
//            logD_ (_func, "c1: ", c);
            return false;
        }
    }
    {
        unsigned long const number = mpz_get_ui (desc) & 0xffffffff;
        mpz_tdiv_q_2exp (desc, desc, 32);
        ret_desc->number = number;
    }
    {
        unsigned long const c = mpz_get_ui (desc) & 0xffff;
        mpz_tdiv_q_2exp (desc, desc, 16);
        if (c != 0xe39a) {
//            logD_ (_func, "c2: ", c);
            return false;
        }
    }
    {
        unsigned long const features = mpz_get_ui (desc) & 0xffffffff;
        mpz_tdiv_q_2exp (desc, desc, 32);
        ret_desc->features = features;
    }
    {
        unsigned long const c = mpz_get_ui (desc) & 0xff;
        mpz_tdiv_q_2exp (desc, desc, 8);
        if (c != 0xb7) {
//            logD_ (_func, "c3: ", c);
            return false;
        }
    }
    {
        unsigned long const date = mpz_get_ui (desc) & 0xffff;
        mpz_tdiv_q_2exp (desc, desc, 16);
        ret_desc->day   = ((date >> 0) & 0x01f) % 32;
        ret_desc->month = ((date >> 5) & 0x00f) % 13;
        ret_desc->year  = ((date >> 9) & 0x01f) + 2013;
    }
    {
        unsigned long const c = mpz_get_ui (desc) & 0xff;
        mpz_tdiv_q_2exp (desc, desc, 8);
        if (c != 0xc4) {
//            logD_ (_func, "c4: ", c);
            return false;
        }
    }

    return true;
}

static void mergeDesc (MomentDesc * const mt_nonnull to,
                       MomentDesc * const mt_nonnull from)
{
    if (to->year < from->year) {
        to->year  = from->year;
        to->month = from->month;
        to->day   = from->day;
    } else
    if (to->year == from->year) {
        if (to->month < from->month) {
            to->month = from->month;
            to->day   = from->day;
        } else
        if (to->month == from->month) {
            if (to->day < from->day)
                to->day = from->day;
        }
    }

    to->features |= from->features;
}

Result setMinPages ()
{
//    logD_ (_func_);

    if (!config_)
        return Result::Success;

    MConfig::Section * const section = config_->getRootSection();
    if (!section)
        return Result::Success;

    {
        MConfig::Section::iterator iter (*section);
        while (!iter.done()) {
            MConfig::SectionEntry * const entry = iter.next ();
            if (entry->getType() == MConfig::SectionEntry::Type_Option) {
                MConfig::Option * const option = static_cast <MConfig::Option*> (entry);
                if (option->getValue())
                    continue;

                entry->getName();
                MomentDesc desc;
                if (!parseConfigEntry (entry->getName(), &desc))
                    return Result::Failure;

#if 0
                logD_ (_func, "day:      ", desc.day);
                logD_ (_func, "month:    ", desc.month);
                logD_ (_func, "year:     ", desc.year);
                logD_ (_func, "number:   ", desc.number);
                logD_ (_func, "features: ", desc.features);
#endif

                mergeDesc (&::Moment::desc, &desc);

                state_flag |= 1;
            }
        }

        if (state_flag & 1) {
            unsigned period = 14;
            bool expired = false;
            bool inverse = false;
            do {
                struct tm tm;
                if (!unixtimeToStructTm (getUnixtime(), &tm)) {
                    logE_ (_func, "unixtimeToStructTm() failed");
                    return Result::Failure;
                }

                unsigned year  = tm.tm_year + 1900;
                unsigned month = tm.tm_mon + 1;
                unsigned day   = tm.tm_mday;

                unsigned const mdays [24] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
                                              31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

                inverse = (year < desc.year) ||
                          (year == desc.year && month < desc.month) ||
                          (year == desc.year && month == desc.month && day < desc.day);

//                logD_ (_func, "inverse: ", inverse);

                unsigned ly, ld, lm,
                         ry, rd, rm;
                if (!inverse) {
                    ly = desc.year; ld = desc.day; lm = desc.month;
                    ry = year;      rd = day;      rm = month;
                } else {
                    ly = year;      ld = day;      lm = month;
                    ry = desc.year; rd = desc.day; rm = desc.month;
                }

//                logD_ (_func, "ly ", ly, ", ld ", ld, ", lm ", lm);
//                logD_ (_func, "ry ", ry, ", rd ", rd, ", rm ", rm);

                if (ry > ly) {
                    if (ry - ly > 1) {
                        expired = true;
                        break;
                    }

                    rm += 12;
                }

                unsigned ndays = 0;
                for (unsigned i = lm; i < rm; ++i)
                    ndays += mdays [i];

                ndays += rd;
                ndays -= ld;

                if (!inverse) {
                    if (ndays <= period)
                        period -= ndays;
                    else
                        expired = true;
                } else {
                    if (ndays > 3)
                        expired = true;
                }
            } while (0);

            if (expired)
                state_flag |= 4;

            if (inverse && expired) {
              // System date is backwards.
//                logD_ (_func, "backwards date: expired: ", expired, ", period: ", period);
                state_flag |= 2;
            } else {
                state_flag |= (period << 3);
            }
        }
    }

    ::Moment::state = ::Moment::desc;
    return Result::Success;

#if 0
    unsigned char const priv_key_buf [] = {
        0x28, 0x31, 0x31, 0x3A,  0x70, 0x72, 0x69, 0x76,  0x61, 0x74, 0x65, 0x2D,  0x6B, 0x65, 0x79, 0x28,
        0x31, 0x34, 0x3A, 0x72,  0x73, 0x61, 0x2D, 0x70,  0x6B, 0x63, 0x73, 0x31,  0x2D, 0x73, 0x68, 0x61,
        0x31, 0x28, 0x31, 0x3A,  0x6E, 0x31, 0x31, 0x33,  0x3A, 0x0B, 0xCC, 0x01,  0x58, 0xF8, 0x06, 0x8F,
        0x62, 0xA7, 0x81, 0x3E,  0x24, 0x3F, 0xE0, 0xD7,  0xCC, 0x50, 0x67, 0xCB,  0xC6, 0x99, 0x4B, 0x7C,
        0xC5, 0x7E, 0x23, 0x02,  0xF3, 0xE2, 0x3B, 0xBA,  0xF7, 0xB6, 0x01, 0x42,  0x10, 0x75, 0x94, 0x7C,
        0x11, 0x04, 0x2B, 0xE1,  0x74, 0xC6, 0xD5, 0xDB,  0xB8, 0xB0, 0x64, 0x3A,  0xAA, 0x26, 0x34, 0x02,
        0x7C, 0x73, 0xC8, 0x37,  0xAA, 0x15, 0xEA, 0x71,  0x0E, 0x3B, 0x00, 0xD0,  0x87, 0x1B, 0xD5, 0x32,
        0xC6, 0x3E, 0xBE, 0x40,  0x55, 0x15, 0x84, 0x04,  0xE2, 0x15, 0x40, 0xDD,  0xF6, 0xE3, 0x88, 0xDA,
        0xFE, 0x04, 0x77, 0x4F,  0x5F, 0x3D, 0x24, 0xD4,  0xB1, 0x49, 0xB0, 0xCC,  0xCB, 0x64, 0xC6, 0xBD,
        0x1A, 0x34, 0xC7, 0xA0,  0xEC, 0x70, 0xFC, 0xCF,  0xBE, 0x3D, 0x29, 0x28,  0x31, 0x3A, 0x65, 0x34,
        0x3A, 0x2F, 0x5E, 0x03,  0x1D, 0x29, 0x28, 0x31,  0x3A, 0x64, 0x31, 0x31,  0x33, 0x3A, 0x03, 0x87,
        0x0A, 0x93, 0x21, 0x3E,  0x64, 0xAD, 0xD2, 0xBD,  0x10, 0x0E, 0x73, 0xE2,  0x05, 0x13, 0x1D, 0x90,
        0x39, 0x90, 0x9E, 0xD4,  0x4F, 0x8E, 0xD5, 0x81,  0x3F, 0xE4, 0xE1, 0x22,  0xF8, 0x3B, 0xCE, 0xC1,
        0x5B, 0x1F, 0x41, 0xEF,  0x22, 0x6F, 0x9C, 0xE6,  0xB8, 0x6D, 0x3F, 0x68,  0x0A, 0x34, 0x4C, 0xB3,
        0x0D, 0xC3, 0x18, 0xF4,  0x17, 0x20, 0xA2, 0x18,  0x88, 0xDE, 0x70, 0xF3,  0xA5, 0xEB, 0xE1, 0x78,
        0xDC, 0x02, 0x65, 0x64,  0x51, 0x7D, 0x85, 0x3A,  0xCD, 0x07, 0xE8, 0x67,  0xE5, 0x67, 0xB6, 0xAD,
        0xC5, 0x6C, 0x8A, 0xEA,  0x5F, 0xB4, 0x12, 0x03,  0xF8, 0x1A, 0xCA, 0x06,  0x38, 0x4C, 0x9F, 0x1B,
        0xF0, 0xBF, 0x8E, 0x8A,  0xF5, 0x72, 0x79, 0x21,  0xEE, 0xB9, 0x27, 0xEC,  0x40, 0x11, 0xB5, 0x29,
        0x28, 0x31, 0x3A, 0x70,  0x35, 0x37, 0x3A, 0x03,  0x98, 0xC9, 0xAA, 0xBA,  0x8C, 0x73, 0x44, 0xAC,
        0xBA, 0x74, 0x01, 0x9F,  0xAF, 0x72, 0x90, 0xD1,  0x4A, 0x1B, 0xD3, 0x84,  0xCA, 0x4E, 0x10, 0xDA,
        0x13, 0xD9, 0xA6, 0x4F,  0x47, 0x48, 0xD4, 0xF3,  0x7B, 0x8F, 0x0E, 0xDA,  0xF3, 0xF7, 0x33, 0x21,
        0xDA, 0xD1, 0x7A, 0xD2,  0x1F, 0xB5, 0xAE, 0x24,  0x00, 0x26, 0x01, 0xA8,  0xF4, 0xA0, 0x4C, 0xDB,
        0x29, 0x28, 0x31, 0x3A,  0x71, 0x35, 0x37, 0x3A,  0x03, 0x47, 0xA1, 0x5E,  0x6A, 0xD2, 0x64, 0x45,
        0xA1, 0x70, 0x81, 0xB4,  0x79, 0x51, 0x0E, 0x79,  0xA0, 0x36, 0x8E, 0x2E,  0x23, 0x68, 0xB6, 0xF3,
        0x09, 0x17, 0x7E, 0xEF,  0xC9, 0x1D, 0xC0, 0xF7,  0xCE, 0x29, 0x0F, 0x1F,  0x84, 0x3A, 0x4D, 0x47,
        0x21, 0xBC, 0xF6, 0x40,  0x0C, 0xB2, 0x5C, 0x17,  0x84, 0xC3, 0x31, 0x4E,  0x80, 0xD8, 0xDC, 0x00,
        0xC7, 0x29, 0x28, 0x31,  0x3A, 0x61, 0x35, 0x36,  0x3A, 0x2D, 0x47, 0xDE,  0x37, 0x03, 0xD8, 0x1C,
        0x38, 0x7A, 0x42, 0xF5,  0xEE, 0x27, 0x78, 0x4D,  0xC8, 0xA4, 0x6D, 0xB1,  0xF0, 0x0B, 0xAA, 0xAE,
        0xED, 0x97, 0x3E, 0x56,  0x60, 0xA7, 0xB8, 0xE2,  0x9A, 0x42, 0xA8, 0x48,  0xD7, 0xD5, 0x7C, 0x72,
        0x6D, 0x33, 0x47, 0xDA,  0x24, 0x3C, 0x79, 0x4A,  0x23, 0x90, 0x82, 0x7B,  0xC5, 0xAB, 0x77, 0x12,
        0x6F, 0x29, 0x28, 0x31,  0x3A, 0x62, 0x35, 0x37,  0x3A, 0x03, 0x35, 0xB4,  0xD9, 0x5A, 0x27, 0xFC,
        0x59, 0xDE, 0x08, 0x75,  0x8B, 0x75, 0xE4, 0xBF,  0xEB, 0x2B, 0x8A, 0xD7,  0xF6, 0x59, 0x4E, 0x5B,
        0x19, 0x54, 0xFA, 0x6A,  0x5D, 0x3E, 0x85, 0x08,  0x1C, 0x8B, 0x0E, 0x82,  0x07, 0xFD, 0x28, 0x53,
        0x4D, 0xB3, 0x75, 0xA0,  0xCF, 0x0E, 0x1D, 0x0C,  0xDD, 0x12, 0xD3, 0x0C,  0xB9, 0xED, 0x15, 0xFE,
        0xD1, 0x8F, 0x29, 0x28,  0x31, 0x3A, 0x63, 0x35,  0x37, 0x3A, 0x01, 0xC5,  0x6B, 0x4F, 0xF9, 0x62,
        0x37, 0xD3, 0xE6, 0x37,  0x77, 0x6C, 0xB4, 0x60,  0xDF, 0x06, 0xA2, 0xD3,  0xA8, 0xFC, 0x96, 0x9D,
        0xBB, 0x63, 0xFB, 0xD5,  0x99, 0x07, 0x03, 0x41,  0xA0, 0x6A, 0x1E, 0xA7,  0x2B, 0xEB, 0x2F, 0xCB,
        0xCF, 0x11, 0x1F, 0x7E,  0xEC, 0x51, 0xF0, 0x69,  0xC0, 0x14, 0x34, 0xB6,  0x36, 0x96, 0x8D, 0x29,
        0x05, 0x63, 0x39, 0x29,  0x29, 0x29
    };

    unsigned char const pub_key_buf [] = {
        0x28, 0x31, 0x30, 0x3A,  0x70, 0x75, 0x62, 0x6C,  0x69, 0x63, 0x2D, 0x6B,  0x65, 0x79, 0x28, 0x31,
        0x34, 0x3A, 0x72, 0x73,  0x61, 0x2D, 0x70, 0x6B,  0x63, 0x73, 0x31, 0x2D,  0x73, 0x68, 0x61, 0x31,
        0x28, 0x31, 0x3A, 0x6E,  0x31, 0x31, 0x33, 0x3A,  0x0B, 0xCC, 0x01, 0x58,  0xF8, 0x06, 0x8F, 0x62,
        0xA7, 0x81, 0x3E, 0x24,  0x3F, 0xE0, 0xD7, 0xCC,  0x50, 0x67, 0xCB, 0xC6,  0x99, 0x4B, 0x7C, 0xC5,
        0x7E, 0x23, 0x02, 0xF3,  0xE2, 0x3B, 0xBA, 0xF7,  0xB6, 0x01, 0x42, 0x10,  0x75, 0x94, 0x7C, 0x11,
        0x04, 0x2B, 0xE1, 0x74,  0xC6, 0xD5, 0xDB, 0xB8,  0xB0, 0x64, 0x3A, 0xAA,  0x26, 0x34, 0x02, 0x7C,
        0x73, 0xC8, 0x37, 0xAA,  0x15, 0xEA, 0x71, 0x0E,  0x3B, 0x00, 0xD0, 0x87,  0x1B, 0xD5, 0x32, 0xC6,
        0x3E, 0xBE, 0x40, 0x55,  0x15, 0x84, 0x04, 0xE2,  0x15, 0x40, 0xDD, 0xF6,  0xE3, 0x88, 0xDA, 0xFE,
        0x04, 0x77, 0x4F, 0x5F,  0x3D, 0x24, 0xD4, 0xB1,  0x49, 0xB0, 0xCC, 0xCB,  0x64, 0xC6, 0xBD, 0x1A,
        0x34, 0xC7, 0xA0, 0xEC,  0x70, 0xFC, 0xCF, 0xBE,  0x3D, 0x29, 0x28, 0x31,  0x3A, 0x65, 0x34, 0x3A,
        0x2F, 0x5E, 0x03, 0x1D,  0x29, 0x29, 0x29
    };

    struct rsa_private_key priv_key;
    rsa_private_key_init (&priv_key);

    struct rsa_public_key pub_key;
    rsa_public_key_init (&pub_key);

    if (!rsa_keypair_from_sexp (NULL /* pub */,
                                &priv_key,
                                0    /* limit */,
                                sizeof (priv_key_buf),
                                priv_key_buf))
    {
        logE_ (_func, "rsa_keypair_from_sexp() failed (priv)");
        return Result::Failure;
    }

    if (!rsa_keypair_from_sexp (&pub_key,
                                NULL /* priv */,
                                0    /* limit */,
                                sizeof (pub_key_buf),
                                pub_key_buf))
    {
        logE_ (_func, "rsa_keypair_from_sexp() failed (pub)");
        return Result::Failure;
    }

    mpz_t plaintext;
    mpz_init (plaintext);
                           //1234567890123456789012345
    mpz_set_str (plaintext, "11223344556677889900aabbc", 16 /* base */);

    mpz_t ciphertext;
    mpz_init (ciphertext);

    mpz_powm (ciphertext, plaintext, pub_key.e, pub_key.n);

    mpz_t decoded;
    mpz_init (decoded);

    rsa_compute_root (&priv_key, decoded, ciphertext);

    char * const n_str = mpz_get_str (NULL /* str */, 16 /* base */, pub_key.n);
    logD_ (_func, "n: ", n_str);

    char * const e_str = mpz_get_str (NULL /* str */, 16 /* base */, pub_key.e);
    logD_ (_func, "e: ", e_str);

    char * const plaintext_str = mpz_get_str (NULL /* str */, 16 /* base */, plaintext);
    logD_ (_func, "P: ", plaintext_str);

    char * const ciphertext_str = mpz_get_str (NULL /* str */, 16 /* base */, ciphertext);
    logD_ (_func, "C: ", ciphertext_str);

    char * const decoded_str = mpz_get_str (NULL /* str */, 16 /* base */, decoded);
    logD_ (_func, "D: ", decoded_str);

    logD_ (_func, "SECOND");
    {
        char const n_str [] = "033843F0E0DE73620C4535DB";
        char const d_str [] = "02125AFDBC997A7EF51603F9";
        char const e_str [] = "3E0E3FD9";

        mpz_t n, d, e;
        mpz_init_set_str (n, n_str, 16);
        mpz_init_set_str (d, d_str, 16);
        mpz_init_set_str (e, e_str, 16);

        mpz_set_str (plaintext, "3ffffffff", 16);
        mpz_powm (ciphertext, plaintext, e, n);

        mpz_powm (decoded, ciphertext, d, n);

        char * const ciphertext_str = mpz_get_str (NULL /* str */, 16 /* base */, ciphertext);
        logD_ (_func, "C: ", ciphertext_str);

        char * const decoded_str = mpz_get_str (NULL /* str */, 16 /* base */, decoded);
        logD_ (_func, "D: ", decoded_str);
    }

    {
        StRef<String> const key = generateKey (1, 2, 3, 4, 5);
        logD_ (_func, "KEY: ", key);
        logD_ (_func, "PARSING");

        if (!parseConfigEntry (key->mem(), &::Moment::desc)) {
            logD_ (_func, "INVALID KEY");
        } else {
            logD_ (_func, "day:      ", ::Moment::desc.day);
            logD_ (_func, "month:    ", ::Moment::desc.month);
            logD_ (_func, "year:     ", ::Moment::desc.year);
            logD_ (_func, "number:   ", ::Moment::desc.number);
            logD_ (_func, "features: ", ::Moment::desc.features);
        }
    }

    return Result::Success;
#endif
}

#if 0
static StRef<String> makeDesc (unsigned const day,
                               unsigned const month,
                               unsigned const year,
                               unsigned const number,
                               unsigned const features)
{
    // 0 1 2 3 4 5 6 7  8 9 a b c d e f  0 1 2 3 4 5 6 7  9 0 a b c d e f  0 1 2
    // d d d d d m m m  m y y y y y n n  n n n n n n n n  n f f f f f f f  f f f

    Uint64 const desc = ((day      & 0x01f) <<  0) |
                        ((month    & 0x00f) <<  5) |
                        ((year     & 0x01f) <<  9) |
                        ((number   & 0x7ff) << 14) |
                        ((features & 0x3ff) << 25);
    return makeString (fmt_hex, desc);

#if 0
    unsigned char desc [5];

    desc [0] = ((day      & 0x01f) <<  0) |
               ((month    & 0x003) <<  5);
    desc [1] = ((month    & 0x008) >>  3) |
               ((year     & 0x01f) <<  1) |
               ((number   & 0x003) <<  6);
    desc [2] = ((number   & 0x3fc) >>  2);
    desc [3] = ((number   & 0x400) >> 10) |
               ((features & 0x07f) <<  1);
    desc [4] = ((features & 0x078) <<  7);

    Format fmt;
    fmt.num_base   = 16;
    fmt.min_digits =  2;
    return makeString (fmt,
                       (unsigned) desc [4],
                       (unsigned) desc [3],
                       (unsigned) desc [2],
                       (unsigned) desc [1],
                       (unsigned) desc [0]);
#endif
}

static void setDesc (Uint64       const desc,
                     MomentDesc * const mt_nonnull ret_desc)
{
    ret_desc->day      = (desc >>  0) & 0x01f;
    ret_desc->month    = (desc >>  5) & 0x00f;
    ret_desc->year     = (desc >>  9) & 0x01f;
    ret_desc->number   = (desc >> 14) & 0x7ff;
    ret_desc->features = (desc >> 25) & 0x3ff;
}
#endif

Result loadConfig_ ()
{
    Byte arr [64];
  #if defined LIBMARY_PLATFORM_WIN32
    // ..\etc\moment-keys.txt
    arr[12]='t'; arr[13]='-'; arr[14]='k';  arr[15]='e';
    arr[20]='x'; arr[21]='t';
    arr[ 4]='t'; arr[ 5]='c'; arr[ 6]='\\'; arr[ 7]='m';
    arr[16]='y'; arr[17]='s'; arr[18]='.';  arr[19]='t';
    arr[ 8]='o'; arr[ 9]='m'; arr[10]='e';  arr[11]='n';
    arr[ 0]='.'; arr[ 1]='.'; arr[ 2]='\\'; arr[ 3]='e';
  #elif defined LIBMARY_PLATFORM_MACOSX
    arr[ 8]='t'; arr[ 9]='i'; arr[10]='o'; arr[11]='n'; arr[12]='s'; arr [13]='/'; arr[14]='M'; arr[15]='o';
    arr[48]='e'; arr[49]='y'; arr[50]='s';
    arr[24]='o'; arr[25]='S'; arr[26]='e'; arr[27]='r'; arr[28]='v'; arr [29]='e'; arr[30]='r'; arr[31]='.';
    arr[40]='m'; arr[41]='o'; arr[42]='m'; arr[43]='e'; arr[44]='n'; arr [45]='t'; arr[46]='-'; arr[47]='k';
    arr[32]='a'; arr[33]='p'; arr[34]='p'; arr[35]='/'; arr[36]='e'; arr [37]='t'; arr[38]='c'; arr[39]='/';
    arr[ 0]='/'; arr[ 1]='A'; arr[ 2]='p'; arr[ 3]='p'; arr[ 4]='l'; arr [ 5]='i'; arr[ 6]='c'; arr[ 7]='a';
    arr[16]='m'; arr[17]='e'; arr[18]='n'; arr[19]='t'; arr[20]='V'; arr [21]='i'; arr[22]='d'; arr[23]='e';
  #else
    // /opt/moment/etc/moment-keys.txt
    // Note that filename is 27 bytes long, which excludes ".txt" extension.
    arr[24]='e'; arr[25]='y'; arr[26]='s'; arr[27]='.'; arr[28]='t'; arr[29]='x'; arr[30]='t';
    arr[ 8]='e'; arr[ 9]='n'; arr[10]='t'; arr[11]='/'; arr[12]='e'; arr[13]='t'; arr[14]='c'; arr[15]='/';
    arr[16]='m'; arr[17]='o'; arr[18]='m'; arr[19]='e'; arr[20]='n'; arr[21]='t'; arr[22]='-'; arr[23]='k';
    arr[ 0]='/'; arr[ 1]='o'; arr[ 2]='p'; arr[ 3]='t'; arr[ 4]='/'; arr[ 5]='m'; arr[ 6]='o'; arr[ 7]='m';
  #endif

    ConstMemory const filename =
            ::Moment::filename_ ?
                    ::Moment::filename_->mem() :
                  #if defined LIBMARY_PLATFORM_WIN32
                    ConstMemory (arr, 22);
                  #elif defined LIBMARY_PLATFORM_MACOSX
                    ConstMemory (arr, 51);
                  #else
                    ConstMemory (arr, 27);
                  #endif

    config_ = grab (new (std::nothrow) MConfig::Config (NULL /* embed_container */));

    bool got_file = false;
    {
        // TODO Use Vfs::stat instead.
        NativeFile file (NULL /* embed_container */);
        if (file.open (filename, 0 /* open_flags */, File::AccessMode::ReadOnly))
            got_file = true;
    }

    if (got_file) {
        if (!MConfig::parseConfig (filename, config_)) {
//            logE_ (_func, "Failed to parse keyfile ", filename);
            return Result::Failure;
        }
    }

    return Result::Success;
}

}

#ifdef MOMENT_STANDALONE_FEATURES
namespace Moment {
static StRef<String> generateKey (unsigned const day,
                                  unsigned const month,
                                  unsigned const year,
                                  unsigned const number,
                                  unsigned const features)
{
  // R2HFJ-UJ7HA-LKW72-KSND7-SH872

    Uint64 const date = ((day   & 0x01f) << 0) |
                        ((month & 0x00f) << 5) |
                        ((year  & 0x07f) << 9);

    Format fmt16;
    fmt16.num_base   = 16;
    fmt16.min_digits =  4;

    Format fmt32;
    fmt32.num_base   = 16;
    fmt32.min_digits =  8;

    // 120 bit
    StRef<String> const desc_str = makeString ("c4",
                                               fmt16, date,
                                               "b7",
                                               fmt32, features,
                                               "e39a",
                                               fmt32, number,
                                               "5d");
//    logD_ (_func, "DESC: ", desc_str);

    mpz_t desc;
    mpz_init_set_str (desc, desc_str->cstr(), 16);

    char const n_str [] = "18C9D5C46C1856C0F2720FE873B29C21";
    char const d_str [] = "0BF32A72C738B5C794716A65C1E20505";
//    char const e_str [] = "3DF779CD";

    mpz_t n, d /*, e */;
    mpz_init_set_str (n, n_str, 16);
    mpz_init_set_str (d, d_str, 16);
//    mpz_init_set_str (e, e_str, 16);

    mpz_t key;
    mpz_init (key);

    mpz_powm (key, desc, d, n);
#if 0
    {
        char * const key_str = mpz_get_str (NULL /* str */, 16 /* base */, key);
        logD_ (_func, "HEX: ", key_str);
    }
#endif

#if 0
    {
        mpz_t decoded;
        mpz_init (decoded);
        mpz_powm (decoded, key, e, n);
        char * const dec_str = mpz_get_str (NULL /* str */, 16 /* base */, decoded);
        logD_ (_func, "DEC: ", dec_str);
    }
#endif

    Uint32 dword [4];
    for (int i = 0; i < 4; ++i) {
        dword [i] = (Uint32) mpz_get_ui (key);
        mpz_tdiv_q_2exp (key, key, 32);

        if (i != 0)
            dword [i] ^= dword [i - 1];
    }
    dword [3] &= 0x1fffffff;

    mpz_set_ui (key, 0);
    for (int i = 0; i < 4; ++i) {
        mpz_add_ui (key, key, dword [3 - i]);
        if (i != 3)
            mpz_mul_2exp (key, key, 32);
    }

#if 0
    {
        char * const key_str = mpz_get_str (NULL /* str */, 16 /* base */, key);
        logD_ (_func, "MANGLED HEX: ", key_str);
    }
#endif

    Byte key_buf [25 + 5];
    unsigned pos = 0;
    for (int i = 0; i < 5; ++i) {
        unsigned long lower =  mpz_get_ui (key);
        mpz_tdiv_q_2exp (key, key, 25);

        for (int j = 0; j < 5; ++j) {
            key_buf [pos] = charmap [lower & 0x1f];
            ++pos;
            lower >>= 5;
        }

        key_buf [pos] = '-';
        ++pos;
    }

    return st_grab (new (std::nothrow) String (ConstMemory (key_buf, sizeof (key_buf) - 1)));
}
} // namespace Moment

using namespace M;
using namespace Moment;

int main (int    argc,
          char **argv)
{
    libMaryInit ();

    if (argc == 2) {
        StRef<String> const key = st_grab (new (std::nothrow) String (argv [1]));
        MomentDesc desc;
        if (!parseConfigEntry (key->mem(), &desc)) {
            logE_ (_func, "Invalid key");
            return EXIT_FAILURE;
        }

        Format fmt;
        fmt.num_base   = 10;
        fmt.min_digits =  2;
        outs->println (desc.year, "/", fmt, desc.month, "/", desc.day, " "
                       "number ", fmt_def, desc.number, ", features 0x", fmt_hex, desc.features);
        return 0;
    }

    if (argc < 3) {
        logD_ (_func, "Not enough arguments");
        return EXIT_FAILURE;
    }

    Uint32 number;
    if (!strToUint32_safe (argv [1], &number, 10 /* base */)) {
        logD_ (_func, "Bad number");
        return EXIT_FAILURE;
    }

    Uint32 features = 0;
    {
        ConstMemory const features_mem (argv [2], strlen (argv [2]));
        if (equal (features_mem, "evaluation")) {
            features = 2;
        } else
        if (equal (features_mem, "commercial")) {
            features = 1;
        } else
        if (equal (features_mem, "rtsp_rtmp")) {
            features = 4;
        } else
        if (equal (features_mem, "rtsp")) {
            features = 6;
        } else {
            logE_ (_func, "Bad features. Epxected: evaluation/commercial");
            return EXIT_FAILURE;
        }

#if 0
// Deprecated
        if (!strToUint32_safe (argv [2], &features, 10 /* base */)) {
            logD_ (_func, "Bad features");
            return EXIT_FAILURE;
        }
#endif
    }

    struct tm tm;
    if (!unixtimeToStructTm (getUnixtime(), &tm)) {
        logE_ (_func, "unixtimeToStructTm() failed");
        return EXIT_FAILURE;
    }

    if (argc >= 6) {
        Uint32 year;
        if (!strToUint32_safe (argv [3], &year, 10 /* base */)) {
            logD_ (_func, "Bad year");
            return EXIT_FAILURE;
        }

        Uint32 month;
        if (!strToUint32_safe (argv [4], &month, 10 /* base */)) {
            logD_ (_func, "Bad month");
            return EXIT_FAILURE;
        }

        Uint32 day;
        if (!strToUint32_safe (argv [5], &day, 10 /* base */)) {
            logD_ (_func, "Bad day");
            return EXIT_FAILURE;
        }

        tm.tm_mday = day;
        tm.tm_mon  = month - 1;
        tm.tm_year = year - 1900;
    }

    logD_ (_func, "d: ", tm.tm_mday, ", m: ", tm.tm_mon + 1, ", y: ", tm.tm_year + 1900);
    logD_ (_func, tm.tm_year + 1900, "/", tm.tm_mon + 1, "/", tm.tm_mday);

    StRef<String> const key = Moment::generateKey (tm.tm_mday,
                                                   tm.tm_mon + 1,
                                                   tm.tm_year + 1900 - 2013,
                                                   number,
                                                   features);
    outs->println (key);

    return 0;
}
#endif /* MOMENT_STANDALONE_FEATURES */

