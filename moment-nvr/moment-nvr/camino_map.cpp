/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/camino_map.h>


namespace MomentNvr {

mt_unlocks_locks (mutex) void
CaminoMap::doInit ()
{
  // TODO

//    StRef<Vfs::VfsDirectory> const dir = Vfs::createDefaultLocalVfs (map_dir);
}

bool
CaminoMap::writeTask (void * const _self)
{
    CaminoMap * const self = static_cast <CaminoMap*> (_self);

    self->mutex.lock ();
    if (!self->initialized) {
        mt_unlocks_locks (mutex) self->doInit ();
    }
    self->initialized = true;
    self->mutex.unlock ();

  // TODO

    return false /* do not reschedule */;
}

void
CaminoMap::beginChunk (ConstMemory const chunk_path)
{
    BeginChunkEntry * const begin_chunk_entry = new (std::nothrow) BeginChunkEntry (chunk_path);
    assert (begin_chunk_entry);

    mutex.lock ();
    begin_chunk_list.append (begin_chunk_entry);
    mutex.unlock ();

    deferred_reg.scheduleTask (&write_task);
}

mt_const void
CaminoMap::init (MomentServer * const mt_nonnull moment,
                 ConstMemory    const map_dir_mem,
                 Uint64         const wrap_chunk_idx)
{
    map_dir = map_dir_mem;
    this->wrap_chunk_idx = wrap_chunk_idx;

    recorder_thread_pool = moment->getRecorderThreadPool ();
    recorder_thread_ctx = recorder_thread_pool->grabThreadContext (map_dir_mem);

    deferred_reg.setDeferredProcessor (recorder_thread_ctx->getDeferredProcessor());
}

CaminoMap::CaminoMap (EmbedContainer * const embed_container)
    : Object         (embed_container),
      wrap_chunk_idx (0),
      initialized    (false),
      next_chunk_idx (0)
{
    write_task.cb = CbDesc<DeferredProcessor::TaskCallback> (writeTask, this, this);
}

CaminoMap::~CaminoMap ()
{
    if (recorder_thread_ctx)
        recorder_thread_pool->releaseThreadContext (recorder_thread_ctx);
}

}

