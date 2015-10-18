/*  Copyright (C) 2011-2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ACTION__H__
#define LIBMARY__ACTION__H__


namespace M {

template <class T>
class NoAction
{
  public:
    static void act (T * const /* obj */) {}
};

template <class T>
class DeleteAction
{
  public:
    static void act (T * const obj) { delete obj; }
};

}


#endif /* LIBMARY__ACTION__H__ */

