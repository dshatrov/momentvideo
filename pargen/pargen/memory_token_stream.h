/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__MEMORY_TOKEN_STREAM__H__
#define PARGEN__MEMORY_TOKEN_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/token_stream.h>


namespace Pargen {

using namespace M;

mt_unsafe class MemoryTokenStream : public TokenStream
{
private:
    mt_const ConstMemory mem;

    mt_const Size max_token_len;
    mt_const bool report_newlines;
    mt_const ConstMemory newline_replacement;
    mt_const bool minus_is_alpha;

    mt_const Byte *token_buf;

    Size cur_pos;

    unsigned long cur_line;
    Uint64 cur_line_start;

public:
  mt_iface (TokenStream)
    mt_throws Result getNextToken    (ConstMemory *ret_mem);
    mt_throws Result getPosition     (PositionMarker * mt_nonnull ret_pmark);
    mt_throws Result setPosition     (PositionMarker const *pmark);
    mt_throws Result getFilePosition (FilePosition *ret_fpos);
  mt_iface_end

    void init (ConstMemory mem,
               bool        report_newlines = false,
               ConstMemory newline_replacement = ConstMemory ("\n"),
               bool        minus_is_alpha  = false,
               Uint64      max_token_len   = 4096);

     MemoryTokenStream ();
    ~MemoryTokenStream ();
};

}


#endif /* PARGEN__MEMORY_TOKEN_STREAM__H__ */

