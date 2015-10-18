/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__COMPARATOR__H__
#define LIBMARY__COMPARATOR__H__


#include <libmary/types.h>
#include <libmary/extractor.h>


namespace M {

/* A comparator should provide the following static methods:
 *
       static bool greater (L const &left, R const &right);
       static bool equals  (L const &left, R const &right);
 */

template < class T,
	   class LeftExtractor = DirectExtractor<T const &>,
	   class RightExtractor = LeftExtractor >
class DirectComparator
{
public:
    static bool greater (T const &left, T const &right)
        { return LeftExtractor::getValue (left) > RightExtractor::getValue (right); }

    static bool equals (T const &left, T const &right)
        { return LeftExtractor::getValue (left) == RightExtractor::getValue (right); }
};

template < class T = ConstMemory const & >
class MemoryComparator
{
public:
    static bool greater (T left, T right)
        { return compare (left, right) == ComparisonResult::Greater; }

    static bool equals (T left, T right)
        { return compare (left, right) == ComparisonResult::Equal; }
};

}


#endif /* LIBMARY__COMPARATOR__H__ */

