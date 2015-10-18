/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__ACCEPTOR__H__
#define PARGEN__ACCEPTOR__H__


#include <libmary/libmary.h>

#include <pargen/parser_element.h>


namespace Pargen {

using namespace M;

class Acceptor : public StReferenced
{
public:
    virtual void setParserElement (ParserElement *parser_element) = 0;
};

template <class T>
class ListAcceptor : public Acceptor
{
protected:
    List<T*> *target_list;

public:
    void setParserElement (ParserElement * const parser_element)
    {
	if (!parser_element)
	    return;

//	assert (dynamic_cast <T*> (parser_element));

	if (target_list)
	    target_list->append (static_cast <T*> (parser_element));
    }

    void init (List<T*> * const target_list)
    {
	this->target_list = target_list;
    }

    ListAcceptor (List<T*> * const target_list)
        : target_list (target_list)
    {
    }

    ListAcceptor ()
    {
    }
};

template <class T>
class PtrAcceptor : public Acceptor
{
protected:
    T **target_ptr;

public:
    void setParserElement (ParserElement * const parser_element)
    {
	if (!parser_element) {
	    if (target_ptr)
		*target_ptr = NULL;

	    return;
	}

	assert (!target_ptr || !*target_ptr);

	if (target_ptr)
	    *target_ptr = parser_element;
    }

    void init (T ** const target_ptr)
    {
	this->target_ptr = target_ptr;

	if (target_ptr)
	    *target_ptr = NULL;
    }

    PtrAcceptor (T ** const target_ptr)
        : target_ptr (target_ptr)
    {
	if (target_ptr)
	    *target_ptr = NULL;
    }

    PtrAcceptor ()
    {
    }
};

template <class T>
class RefAcceptor : public Acceptor
{
protected:
    StRef<T> *target_ref;

public:
    void setParserElement (ParserElement * const parser_element)
    {
	if (!parser_element) {
	    if (target_ref)
		*target_ref = NULL;

	    return;
	}

        assert (!target_ref || !*target_ref);

	if (target_ref)
	    *target_ref = parser_element;
    }

    void init (StRef<T> * const target_ref)
    {
	this->target_ref = target_ref;

	if (target_ref)
	    *target_ref = NULL;
    }

    RefAcceptor (StRef<T> * const target_ref)
        : target_ref (target_ref)
    {
	if (target_ref)
	    *target_ref = NULL;
    }

    RefAcceptor ()
    {
    }
};

}


#endif /* PARGEN__ACCEPTOR__H__ */

