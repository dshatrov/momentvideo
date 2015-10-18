/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__FILE_POSITION__H__
#define PARGEN__FILE_POSITION__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

class FilePosition
{
public:
	   // Current line number
    Uint64 line,
	   // Absolute position of the beginning of the current line
	   line_pos,
	   // Absoulute position of the current character
	   char_pos;

    Size toString_ (Memory const &mem, Format const & /* fmt */) const
        { return printToString (mem, line, ":", char_pos - line_pos); }

    FilePosition (Uint64 line,
		  Uint64 line_pos,
		  Uint64 char_pos)
	: line (line),
	  line_pos (line_pos),
	  char_pos (char_pos)
    {
    }

    FilePosition ()
	: line (0),
	  line_pos (0),
	  char_pos (0)
    {
    }
};

}


#endif /* PARGEN__FILE_POSITION__H__ */

