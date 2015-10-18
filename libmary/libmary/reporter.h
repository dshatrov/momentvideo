/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__REPORTER_H__
#define LIBMARY__REPORTER_H__


#include <libmary/intrusive_avl_tree.h>
#include <libmary/state_mutex.h>


namespace M {

class ReporterThreadContext : public DependentCodeReferenced
{
private:
    mt_const Uint64 thread_id;
    mt_const DataDepRef<DeferredProcessor> deferred_processor;

public:
    Uint64 getThreadId () const { return thread_id; }
    DeferredProcessor* getDeferredProcessor () const { return deferred_processor; }

    mt_const void init (Uint64              const thread_id,
                        DeferredProcessor * const mt_nonnull deferred_processor)
    {
        this->thread_id = thread_id;
        this->deferred_processor = deferred_processor;
    }

    ReporterThreadContext (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          thread_id               (0),
          deferred_processor      (coderef_container)
    {}
};

class Reporter : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    typedef void ReportCallback (void *cb_data);

#if 0
    typedef void ReportCallback (void *cb,
                                 void *cb_data /* ,
                                 VirtReferenced *ret_data */);
#endif

private:
    class Subscription : public IntrusiveListElement<>
    {
    public:
        void *cb;
        void *cb_data;
        WeakCodeRef weak_code_ref;
        VirtRef ref_data;

        CodeDepRef<ReporterThreadContext> reporter_thread_ctx;
    };

    class ReporterThread : public IntrusiveAvlTreeNode<>
    {
    public:
        mt_const UintPtr thread_id;
        mt_const CodeDepRef<DeferredProcessor> deferred_processor;

        DeferredProcessor::Task deferred_task;
        DeferredProcessor::Registration deferred_reg;
    };

    typedef IntrusiveAvlTree< ReporterThread,
                              MemberExtractor< ReporterThread,
                                               UintPtr,
                                               &ReporterThread::thread_id > >
            ReporterThreadTree;

    mt_mutex (mutex) ReporterThreadTree thread_tree;

    void subscribeVoid (ReporterThreadContext * mt_nonnull reporter_thread_ctx,
                        void                  *cb,
                        void                  *cb_data,
                        VirtReferenced        *ref_data,
                        Object                *coderef_container);

public:
    template <class T>
    void subscribe (ReporterThreadContext * const mt_nonnull reporter_thread_ctx,
                    CbDesc<T> const &cb)
    {
        subscribeVoid (reporter_thread_ctx,
                       (void*) cb.cb,
                       cb.cb_data,
                       cb.ref_data,
                       cb.coderef_container);
    }

    void report (ReportCallback report_cb,);

    mt_const void init (CbDesc<>);

    Reporter (Object *coderef_container);
};

}


#endif /* LIBMARY__REPORTER_H__ */

