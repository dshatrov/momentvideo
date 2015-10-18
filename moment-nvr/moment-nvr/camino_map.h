/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__CAMINO_MAP__H__
#define MOMENT_NVR__CAMINO_MAP__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace Moment;

class CaminoMap : public Object
{
  private:
    StateMutex mutex;

    class BeginChunkEntry : public IntrusiveListElement<>
    {
      public:
        String chunk_path;

        BeginChunkEntry (ConstMemory const chunk_path_mem)
            : chunk_path (chunk_path_mem)
        {}
    };

    mt_const String map_dir;
    mt_const Uint64 wrap_chunk_idx;

    mt_const Ref<ServerThreadPool>    recorder_thread_pool;
    mt_const Ref<ServerThreadContext> recorder_thread_ctx;

    mt_mutex (mutex) bool initialized;
    mt_mutex (mutex) Uint64 next_chunk_idx;

    typedef IntrusiveList<BeginChunkEntry, IntrusiveList_name, DeleteAction<BeginChunkEntry> > BeginChunkList;
    mt_mutex (mutex) BeginChunkList begin_chunk_list;

    DeferredProcessor::Task write_task;
    DeferredProcessor::Registration deferred_reg;

    mt_unlocks_locks (mutex) void doInit ();

    static bool writeTask (void *_self);

  public:
    void beginChunk (ConstMemory chunk_path);

    mt_const void init (MomentServer * mt_nonnull moment,
                        ConstMemory   map_dir_mem,
                        Uint64        wrap_chunk_idx);

     CaminoMap (EmbedContainer *embed_container);
    ~CaminoMap ();
};

}


#endif /* MOMENT_NVR__CAMINO_MAP__H__ */

