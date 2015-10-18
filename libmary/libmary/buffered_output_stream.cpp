/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/buffered_output_stream.h>


// NOTE: Don't call log*() functions here. BufferedOutputStream is a part of logging chain.


namespace M {

mt_throws Result
BufferedOutputStream::flushBuffer ()
{
    Size towrite = in_buffer;
    Size total_written = 0;
    while (towrite > 0) {
        Size nwritten;
        if (!out->write (ConstMemory (data_buf + total_written, towrite), &nwritten)) {
            // Discarding all remaining data.
            in_buffer = 0;
            return Result::Failure;
        }

        assert (nwritten <= towrite);
        towrite -= nwritten;
        total_written += nwritten;
    }

    in_buffer = 0;
    return Result::Success;
}

mt_throws Result
BufferedOutputStream::write (ConstMemory   const mem,
                             Size        * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    Byte const *src_buf = mem.mem();
    Size src_len = mem.len();

    for (;;) {
        Size towrite = src_len;
        if (data_len - in_buffer < towrite)
            towrite = data_len - in_buffer;

        memcpy (data_buf + in_buffer, src_buf, towrite);
        in_buffer += towrite;

        if (towrite == src_len)
            break;

        if (!flushBuffer ())
            return Result::Failure;

        src_buf += towrite;
        src_len -= towrite;
    }

    if (ret_nwritten)
        *ret_nwritten = mem.len();

    return Result::Success;
}

mt_throws Result
BufferedOutputStream::flush ()
{
    if (!flushBuffer ())
        return Result::Failure;

    return out->flush ();
}

BufferedOutputStream::BufferedOutputStream (OutputStream * const mt_nonnull out,
                                            Size           const buf_len)
    : out       (out),
      data_buf  (new (std::nothrow) Byte [buf_len]),
      data_len  (buf_len),
      in_buffer (0)
{
    assert (data_buf);
}

BufferedOutputStream::~BufferedOutputStream ()
{
    delete[] data_buf;
}

}

