/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__FILE__H__
#define LIBMARY__FILE__H__


#include <libmary/input_stream.h>
#include <libmary/output_stream.h>


namespace M {

class File : public InputStream,
             public OutputStream
{
  public:
    typedef M::FileAccessMode AccessMode;
    typedef M::FileOpenFlags  OpenFlags;

    typedef M::FileType FileType;
    typedef M::FileStat FileStat;

    virtual mt_throws Result seek (FileOffset offset,
                                   SeekOrigin origin) = 0;

    mt_throws Result seekBeg (FileSize offset);

    virtual mt_throws Result tell (FileSize *ret_pos) = 0;

    virtual mt_throws Result sync () = 0;

    virtual mt_throws Result close (bool flush_data = true) = 0;

    #ifdef LIBMARY_ENABLE_MWRITEV
      virtual int getFd () = 0;
    #endif

    virtual ~File () {}
};

}


#endif /* LIBMARY__FILE__H__ */

