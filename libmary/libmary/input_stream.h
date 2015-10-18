/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__INPUT_STREAM__H__
#define LIBMARY__INPUT_STREAM__H__


#include <libmary/exception.h>


namespace M {

class InputStream : public virtual Referenced
{
  public:
    virtual mt_throws IoResult read (Memory  mem,
                                     Size   *ret_nread) = 0;

  // Non-virtual methods

    mt_throws IoResult readFull (Memory  mem,
                                 Size   *ret_nread);

    virtual ~InputStream () {}
};

}


#endif /* LIBMARY__INPUT_STREAM__H__ */

