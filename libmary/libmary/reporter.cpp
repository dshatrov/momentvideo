/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/reporter.h>


namespace M {

void
Reporeter::subscribeVoid (ReporterThreadContext * const mt_nonnull reporter_thread_ctx,
                          void                  * const cb,
                          void                  * const cb_data,
                          VirtReferenced        * const ref_data,
                          Object                * const coderef_container)
{
    ReporterThread *repoter_thread = thread_tree.lookup (reporter_thread_ctx->getThreadId());
    if (!reporter_thread) {
        reporter_thread = new (std::nothrow) ReporterThread;
        reporter_thread->thread_id = reporter_thread_ctx->getThreadId();
        reporter_thread->deferred_processor = reporter_thread->getDeferredProcessor();
    }
}

}

