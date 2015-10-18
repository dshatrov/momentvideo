/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__UTIL__H__
#define SCRUFFY__UTIL__H__


#include <libmary/libmary.h>


namespace Scruffy {

using namespace M;

struct StringNumberPair
{
    const char    *string;
    unsigned long  number;
};

void validateStringNumberPairs (const StringNumberPair *pairs,
				unsigned long           num_pairs)
			 throw (InternalException);

bool matchStringNumberPairs (const char             *str,
			     unsigned long          *ret_number,
			     const StringNumberPair *pairs,
			     unsigned long           num_pairs);

void validateStrings (const char    **strs,
		      unsigned long   num_strs)
	       throw (InternalException);

unsigned long matchStrings (const char     *str,
			    const char    **strs,
			    unsigned long   num_strs);

bool numbersAreZero (const unsigned long *number_set,
		     unsigned long        nnumbers);

bool numberIsMaxOf (unsigned long        number,
		    const unsigned long *number_set,
		    unsigned long        nnumbers);

unsigned long maxNumberOf (const unsigned long *number_set,
			   unsigned long        nnumbers);

}


#endif /* SCRUFFY__UTIL__H__ */

