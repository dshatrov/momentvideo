/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ARRAY_HOLDER__H__
#define LIBMARY__ARRAY_HOLDER__H__


#include <libmary/types_base.h>
#include <libmary/util_base.h>


namespace M {

template <class T>
class ArrayHolder
{
private:
    T *array;

    ArrayHolder& operator = (const ArrayHolder &);
    ArrayHolder (const ArrayHolder &);

public:
/* This is not necessary because of the presense of 'operator T*'.
    T& operator [] (const size_t index) const
    {
	return array [index];
    }
*/

    operator T* ()
    {
	return array;
    }

    void releasePointer ()
    {
	array = NULL;
    }

    void setPointer (T *new_array)
    {
	if (array != NULL)
	    delete[] array;

	array = new_array;
    }

    void allocate (unsigned long size)
    {
	if (array != NULL)
	    delete[] array;

	if (size > 0) {
	    array = new T [size];
	    assert (array);
	} else
	    array = NULL;
    }

    void deallocate ()
    {
	if (array != NULL)
	    delete[] array;

	array = NULL;
    }

    ArrayHolder ()
    {
	array = NULL;
    }

    ArrayHolder (T *array)
    {
	this->array = array;
    }

    ArrayHolder (unsigned long size)
    {
	if (size > 0) {
	    array = new T [size];
	    assert (array);
	} else {
	    array = NULL;
	}
    }

    ~ArrayHolder ()
    {
	if (array != NULL)
	    delete[] array;
    }
};

}


#endif /* LIBMARY__ARRAY_HOLDER__H__ */

