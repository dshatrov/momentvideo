/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__PARSING_EXCEPTION__H__
#define SCRUFFY__PARSING_EXCEPTION__H__


#include <libmary/libmary.h>

#include <pargen/file_position.h>


namespace Scruffy {

using namespace M;

class ParsingException : public Exception
{
public:
    Pargen::FilePosition const fpos;
    StRef<String> message;

    StRef<String> toString ()
    {
        if (cause)
            return makeString ("ParsingException: ", message->mem(), ": ", cause->toString()->mem());
        else
            return makeString ("ParsingException: ", message->mem());
    }

    ParsingException (Pargen::FilePosition const &fpos,
		      String * const message)
        : fpos (fpos),
          message (message)
    {
        if (!this->message)
            this->message = st_grab (new (std::nothrow) String);
    }
};

}


#endif /* SCRUFFY__PARSING_EXCEPTION__H__ */

