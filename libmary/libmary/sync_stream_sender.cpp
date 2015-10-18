/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/sync_stream_sender.h>


namespace M {

void
SyncStreamSender::sendVector (IovArray * const mt_nonnull iovs,
                              bool       const do_flush)
{
    {
        Size nwritten = 0;
        write_mutex.lock ();
        if (!output_stream->writevFull (iovs->elems, iovs->i, &nwritten)) {
            write_mutex.unlock ();
            close ();
            return;
        }
        write_mutex.unlock ();
    }

    if (do_flush) {
        if (!output_stream->flush ()) {
            close ();
            return;
        }
    }
}

void
SyncStreamSender::flush ()
{
    if (!output_stream->flush ())
        close ();
}

mt_mutex (mutex) void
SyncStreamSender::flush_locked ()
{
    if (!output_stream->flush ())
        close ();
}

void
SyncStreamSender::closeAfterFlush ()
{
    // Debatable. Waiting for all concurrent invocations of sendVector()
    // to complete would probably be safer.
    close ();
}

void
SyncStreamSender::close ()
{
//#error TODO lock the mutex and set is_closed

    fireClosed_deferred (&deferred_reg, NULL /* exc_buf */);
    if (frontend) {
        frontend.call_deferred (&deferred_reg,
                                frontend->closed,
                                NULL /* extra_ref_data */,
                                static_cast <Exception*> (NULL) /* exc_ */);
    }
}

}

