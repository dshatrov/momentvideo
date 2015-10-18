/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__FILE_TOKEN_STREAM__H__
#define PARGEN__FILE_TOKEN_STREAM__H__


#include <libmary/libmary.h>

#include <pargen/token_stream.h>


namespace Pargen {

using namespace M;

class FileTokenStream : public TokenStream
{
private:
    File *file;

    bool const report_newlines;
    bool const minus_is_alpha;

    unsigned long cur_line;
    Uint64 cur_line_start;
    Uint64 cur_line_pos;
    Uint64 cur_char_pos;

    Byte *token_buf;
    Size token_len;
    Size const max_token_len;

public:
  mt_iface (TokenStream)
    mt_throws Result getNextToken (ConstMemory *ret_mem);

    mt_throws Result getPosition (PositionMarker * mt_nonnull ret_pmark);

    mt_throws Result setPosition (PositionMarker const *pmark);

    mt_throws Result getFilePosition (FilePosition *ret_fpos);

#if 0
    unsigned long getLine ();

    StRef<String> getLineStr ()
		     throw (InternalException);
#endif
  mt_iface_end

    FileTokenStream (File * mt_nonnull file,
		     bool  report_newlines = false,
                     bool  minus_is_alpha  = false,
                     Size  max_token_len = 4096);

    ~FileTokenStream ();
};

}


#endif /* PARGEN__FILE_TOKEN_STREAM__H__ */

