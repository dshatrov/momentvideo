/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__EXTRACTOR__H__
#define LIBMARY__EXTRACTOR__H__


#include <libmary/pointer.h>


namespace M {

template <class T>
class DirectExtractor
{
public:
    static T getValue (T t) { return t; }
};

template < class T,
	   class C >
class CastExtractor
{
public:
    static C getValue (T t) { return (C) t; }
};

template < class T,      // The type of an aggregate which we are extracting from
	   class X,      // The type of an element to extract
	   X T::*m,      // Pointer to the element being extracted
	   class E = X&, // Return type of the extractor
	   class Extractor = DirectExtractor<X&> >
class MemberExtractor
{
public:
    template <class C>
    static E getValue (C &c) { return Extractor::getValue ((* Pointer<T> (c)).*m); }
};

template < class T,
	   class X,
	   X (T::*m) () const,
	   class E = X,
	   class Extractor = DirectExtractor<X> >
class AccessorExtractor
{
public:
    template <class C>
    static E getValue (C &c) { return Extractor::getValue (((* Pointer<T> (c)).*m) ()); }
};

// Useful when referring to accessors of parent classes.
// Ambiguity in constructor call Pointer<T>() does not allow
// to use the original AccessorExtractor template in such cases.
//
template < class B,
	   class T,
	   class X,
	   X (T::*m) () const,
	   class E = X,
	   class Extractor = DirectExtractor<X> >
class AccessorExtractorEx
{
public:
    template <class C>
    static E getValue (C &c) { return Extractor::getValue (((* Pointer<B> (c)).*m) ()); }
};

template < class T,
	   class E = T&,
	   class Extractor = DirectExtractor<T&> >
class DereferenceExtractor
{
public:
    template <class C>
    static E getValue (C &c) { return Extractor::getValue (* Pointer<T> (c)); }
};

// Note: So far, this class is unused.
template < class T,
	   class E = T*,
	   class Extractor = DirectExtractor<T> >
class AddressExtractor
{
public:
    static E getValue (T &t) { return Extractor::getValue (&t); }
};

}


#endif /* LIBMARY__EXTRACTOR__H__ */

