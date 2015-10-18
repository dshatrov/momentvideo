/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__FILE_BYTE_STREAM__H__
#define SCRUFFY__FILE_BYTE_STREAM__H__


#include <libmary/libmary.h>

#include <scruffy/byte_stream.h>


namespace Scruffy {

using namespace M;

class FileByteStream : public ByteStream
{
protected:
    class PositionMarker : public ByteStream::PositionMarker
    {
    public:
	FileSize offset;
    };

    File *in_file;
    FileSize start_offset;

public:
  /* ByteStream interface */
  mt_iface (ByteStream)

    ByteResult getNextByte (char *c)
                     throw (InternalException);

    StRef<ByteStream::PositionMarker> getPosition ()
                                            throw (InternalException);

    void setPosition (ByteStream::PositionMarker *pmark)
               throw (InternalException);

  mt_iface_end

    FileByteStream (File *in_file)
	     throw (InternalException);
};

}


#endif /* SCRUFFY__FILE_BYTE_STREAM__H__ */

