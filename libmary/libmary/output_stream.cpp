/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/util_common.h>

#include <libmary/output_stream.h>


// NOTE: Don't call log*() functions here. OutputStream is a part of logging chain.


namespace M {

mt_throws Result
OutputStream::writev (IovArrayElement * const iovs,
		      Count             const num_iovs,
		      Size            * const ret_nwritten)
{
    if (ret_nwritten)
	*ret_nwritten = 0;

    Size total_written = 0;
    for (Count i = 0; i < num_iovs; ++i) {
	Size nwritten;
	Result const res = write (iovArrayElementMem (&iovs [i]), &nwritten);
	total_written += nwritten;
	if (!res) {
	    if (ret_nwritten)
		*ret_nwritten = total_written;

	    return res;
	}
    }

    if (ret_nwritten)
	*ret_nwritten = total_written;

    return Result::Success;
}

mt_throws Result
OutputStream::writevFull (IovArrayElement * const iovs,
                          Count             const num_iovs,
                          Size            * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    Size nwritten = 0;
    if (!writev (iovs, num_iovs, &nwritten)) {
        if (ret_nwritten)
            *ret_nwritten = nwritten;

        return Result::Failure;
    }

    Size total_len = 0;
    for (Count i = 0; i < num_iovs; ++i)
        total_len += iovArrayElementLen (&iovs [i]);

    if (nwritten < total_len)
        return writevFull_tail (iovs, num_iovs, total_len, nwritten, ret_nwritten);

    if (ret_nwritten)
        *ret_nwritten = nwritten;

    return Result::Success;
}

// Stack-hungry extension for writevFull()
mt_throws Result
OutputStream::writevFull_tail (IovArrayElement * iovs,
                               Count            num_iovs,
                               Size              const total_len,
                               Size             total_written,
                               Size            * const ret_nwritten)
{
    // Stack-hungry part
    IovArrayElement iov_elems [LIBMARY__IOV_MAX];

    if (ret_nwritten)
        *ret_nwritten = total_written;

    Size tocut = total_written;
    Size offset = 0;
    for (;;) {
        assert (num_iovs > 0);
        {
            bool first = true;
            while (tocut > 0) {
                Size elem_len = iovArrayElementLen (iovs);
                if (first) {
                    assert (elem_len > offset);
                    elem_len -= offset;
                }

                if (elem_len > tocut) {
                    if (first)
                        offset += tocut;
                    else
                        offset = tocut;

                    break;
                }

                tocut -= iovArrayElementLen (iovs);
                ++iovs;
                --num_iovs;
                assert (num_iovs > 0);

                first = false;
                offset = 0;
            }
        }

        Count i = 0;
        // This may be done more efficiently (full copy just once).
        for (; i < num_iovs && i < LIBMARY__IOV_MAX; ++i)
            iov_elems [i] = iovs [i];

        assert (iovArrayElementLen (iov_elems) >= offset);
        iovArrayElementSet (iov_elems,
                            iovArrayElementBuf (iov_elems) + offset,
                            iovArrayElementLen (iov_elems) - offset);

        {
            Size nwritten = 0;
            if (!writev (iov_elems, i, &nwritten)) {
                if (ret_nwritten)
                    *ret_nwritten = total_written + nwritten;

                return Result::Failure;
            }

            total_written += nwritten;
            assert (total_written <= total_len);
            if (total_written == total_len)
                break;

            tocut = nwritten;
        }
    }

    if (ret_nwritten)
        *ret_nwritten = total_written;

    return Result::Success;
}

mt_throws Result
OutputStream::writeFull (ConstMemory   const mem,
			 Size        * const nwritten)
{
    return writeFull_common (this, mem, nwritten);
}

}

