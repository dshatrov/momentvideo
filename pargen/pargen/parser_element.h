/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__PARSER_ELEMENT__H__
#define PARGEN__PARSER_ELEMENT__H__


#include <libmary/libmary.h>


#define VSLAB_ACCEPTOR


namespace Pargen {

using namespace M;

class ParserElement
  #ifndef VSLAB_ACCEPTOR
    : public StReferenced
  #endif
{
public:
    void *user_obj;

    ParserElement ()
	: user_obj (NULL)
    {}
};

class ParserElement_Token : public ParserElement
{
public:
    ConstMemory token;

    ParserElement_Token (ConstMemory   const token,
			 void        * const user_obj)
	: token (token)
    {
	this->user_obj = user_obj;
    }
};

}


#endif /* PARGEN__PARSER_ELEMENT__H__ */

