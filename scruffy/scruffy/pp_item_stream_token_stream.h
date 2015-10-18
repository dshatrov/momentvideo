/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__PP_ITEM_STREAM_TOKEN_STREAM__H__
#define SCRUFFY__PP_ITEM_STREAM_TOKEN_STREAM__H__


#include <pargen/token_stream.h>

#include <scruffy/pp_item_stream.h>


namespace Scruffy {

using namespace M;

class PpItemStreamTokenStream : public Pargen::TokenStream
{
protected:
#if 0
    class PositionMarker : public Pargen::TokenStream::PositionMarker
    {
    public:
	StRef<PpItemStream::PositionMarker> pp_pmark;
    };
#endif

    StRef<PpItemStream> pp_stream;
    StRef<PpItemStream::PositionMarker> cur_pmark;

    StRef<String> newline_replacement;

public:
  mt_iface (Pargen::TokenStream)

    mt_throws Result getNextToken (ConstMemory *ret_mem);

    mt_throws Result getNextToken (ConstMemory          *ret_mem,
                                   StRef<StReferenced>  *ret_user_obj,
                                   void               **ret_user_ptr);

    mt_throws Result getPosition (Pargen::TokenStream::PositionMarker *ret_pmark /* non-null */);

    mt_throws Result setPosition (Pargen::TokenStream::PositionMarker const *pmark /* non-null */);

    mt_throws Result getFilePosition (Pargen::FilePosition *ret_fpos)
    {
        *ret_fpos = Pargen::FilePosition ();
        return Result::Success;
    }

  mt_iface_end

    void setNewlineReplacement (ConstMemory const &replacement)
    {
	newline_replacement = st_grab (new (std::nothrow) String (replacement));
    }

    PpItemStreamTokenStream (PpItemStream *pp_stream);
};

}


#endif /* SCRUFFY__PP_ITEM_STREAM_TOKEN_STREAM__H__ */

