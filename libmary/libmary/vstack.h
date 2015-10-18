/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__VSTACK__H__
#define LIBMARY__VSTACK__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>


namespace M {

mt_unsafe class VStack
{
public:
    typedef Size Level;

private:
    // TODO Use one malloc for Block+buf,
    //      with buf allocated right after Block.
    //      Don't forget about alignment requirements
    //      for the first data block in buf.
    class Block : public IntrusiveListElement<>
    {
    public:
	Byte *buf;

	Size start_level;
	Size height;
    };

    typedef IntrusiveList<Block> BlockList;

    Size const block_size;
    bool const shrinking;

    Size level;

    BlockList block_list;
    Block *cur_block;

    Byte* addBlock (Size num_bytes);

public:
    Byte* push_unaligned (Size num_bytes);

    Byte* push_malign (Size num_bytes,
                       Size alignment);

    Level getLevel () const { return level; }

    void setLevel (Level new_level);

    void clear () { setLevel (0); }

    VStack (Size block_size /* > 0 */,
	    bool shrinking = false);

    ~VStack ();
};

}


#endif /* LIBMARY__VSTACK__H__ */

