/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>
#include <moment/libmoment.h>
#include <moment/moment_instance.h>


using namespace Moment;

namespace {
class MomentVodModule : public Object
{
  private:
    struct ProviderEntry : public Referenced
    {
        MomentVodModule *moment_vod_module;
        StRef<String> file_path;
    };

    mt_const Ref<PagePool> page_pool;
    mt_const Ref<ServerThreadPool> reader_thread_pool;

    struct SourceDeletedData : public Referenced
    {
        MomentVodModule          *moment_vod_module;
        Ref<ServerThreadContext>  thread_ctx;
    };

    static void readerSourceDeleted (void *_data);

    Ref<MediaSource> openMediaFile (ConstMemory filename);

    mt_const void addPathEntry (MConfig::Section * mt_nonnull section,
                                MediaManager     * mt_nonnull media_manager);

    mt_const void initConfig (MomentServer * mt_nonnull moment);

  public:
    mt_iface (MediaManager::MediaProvider)
      static MediaManager::MediaProvider const media_provider;

      static Ref<MediaSource> getMediaSource (ConstMemory  path,
                                              void        *_self);
    mt_iface_end

    mt_const void init (MomentServer * mt_nonnull moment);

    MomentVodModule (EmbedContainer *embed_container);
};
}

void
MomentVodModule::readerSourceDeleted (void * const _data)
{
    SourceDeletedData * const data = static_cast <SourceDeletedData*> (_data);
    MomentVodModule * const self = data->moment_vod_module;

    self->reader_thread_pool->releaseThreadContext (data->thread_ctx);
}

Ref<MediaSource>
MomentVodModule::openMediaFile (ConstMemory const filename)
{
    logD_ (_this_func, "filename \"", filename, "\"");

    StRef<Mp4Reader> const mp4_reader = st_grabNew <Mp4Reader> ();
    mp4_reader->init (page_pool);
    if (!mp4_reader->open (filename)) {
        logE_ (_this_func, "mp4_reader.open() failed");
        return NULL;
    }

    Ref<ReaderVodSource> const reader_vod_source = grabNewObject <ReaderVodSource> ();

    Ref<ServerThreadContext> const thread_ctx = reader_thread_pool->grabThreadContext (filename);
    {
        Ref<SourceDeletedData> const data = grabNew <SourceDeletedData> ();
        data->moment_vod_module = this;
        data->thread_ctx = thread_ctx;

        reader_vod_source->addDeletionCallback (
                CbDesc<Object::DeletionCallback> (readerSourceDeleted, data, this, data));
    }

    reader_vod_source->init (thread_ctx->getDeferredProcessor(),
                             thread_ctx->getTimers(),
                             mp4_reader);
    return reader_vod_source;
}

MediaManager::MediaProvider const MomentVodModule::media_provider = {
    getMediaSource
};

Ref<MediaSource>
MomentVodModule::getMediaSource (ConstMemory  path,
                                 void        * const _provider_entry)
{
    ProviderEntry * const provider_entry = static_cast <ProviderEntry*> (_provider_entry);
    MomentVodModule * const self = provider_entry->moment_vod_module;

    logD_ (_self_func "path: \"", path, "\"");

    path = stripSlashes (path);
    return self->openMediaFile (makeString (provider_entry->file_path, "/", path));
}

mt_const void
MomentVodModule::addPathEntry (MConfig::Section * const mt_nonnull section,
                               MediaManager     * const mt_nonnull media_manager)
{
    ConstMemory file_path;
    if (MConfig::Option * const opt = section->getOption ("path")) {
        if (MConfig::Value * const val = opt->getValue())
            file_path = val->mem();
    }
    file_path = stripTrailingSlashes (file_path);

    ConstMemory prefix;
    if (MConfig::Option * const opt = section->getOption ("prefix")) {
        if (MConfig::Value * const val = opt->getValue())
            prefix = val->mem();
    }
    prefix = stripSlashes (prefix);

    logD_ (_this_func, "adding path entry: file_path \"", file_path, "\", prefix \"", prefix, "\"");

    Ref<ProviderEntry> const provider_entry = grabNew <ProviderEntry> ();
    provider_entry->moment_vod_module = this;
    provider_entry->file_path = newString (file_path);

    media_manager->addMediaProvider (
            prefix,
            CbDesc<MediaManager::MediaProvider> (&media_provider, provider_entry, this, provider_entry));
}

mt_const void
MomentVodModule::initConfig (MomentServer * const mt_nonnull moment)
{
    Ref<MConfig::Config> const config = moment->getConfigManager()->getConfig();

    MConfig::Section * const modvod_section = config->getSection ("mod_vod");
    if (!modvod_section)
        return;

    MConfig::Section::iterator iter (*modvod_section);
    while (!iter.done()) {
        MConfig::SectionEntry * const sect_entry = iter.next ();
        if (sect_entry->getType() == MConfig::SectionEntry::Type_Section) {
            MConfig::Section * const section = static_cast <MConfig::Section*> (sect_entry);
            if (equal (sect_entry->getName(), "dir"))
                addPathEntry (section, moment->getMediaManager());
        }
    }
}

mt_const void
MomentVodModule::init (MomentServer * const mt_nonnull moment)
{
    page_pool = moment->getPagePool();
    reader_thread_pool = moment->getReaderThreadPool();

    initConfig (moment);
}

MomentVodModule::MomentVodModule (EmbedContainer * const embed_container)
    : Object (embed_container)
{
}

namespace MomentVod {

Result momentVodInit (MomentInstance * const mt_nonnull moment_instance)
{
    logD_ (_func_);

    Ref<MomentVodModule> const moment_vod_module = grabNewObject <MomentVodModule> ();
    moment_vod_module->init (moment_instance->getMomentServer());

    moment_instance->addDeletionData (moment_vod_module);
    return Result::Success;
}

}

#ifndef MOMENT_STATIC_MODULES
  extern "C" bool
  libMary_moduleInit (void * const moment_instance_)
          { return MomentVod::momentVodInit (static_cast <MomentInstance*> (moment_instance_)); }
#endif

