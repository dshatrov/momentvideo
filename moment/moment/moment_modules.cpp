#include <moment/libmoment.h>
#include <moment/moment_modules.h>


#ifdef MOMENT_STATIC_MODULES
  using namespace Moment;

  namespace MomentTest  { Result momentTestInit (MomentInstance * mt_nonnull moment_instance); }
  namespace MomentFile  { Result momentFileInit (MomentInstance * mt_nonnull moment_instance); }
  namespace MomentVod   { Result momentVodInit  (MomentInstance * mt_nonnull moment_instance); }
  namespace MomentAuth  { Result momentAuthInit (MomentInstance * mt_nonnull moment_instance); }
  namespace MomentRtmp  { Result momentRtmpInit (MomentInstance * mt_nonnull moment_instance); }
  namespace MomentRtsp  { Result momentRtspInit (MomentInstance * mt_nonnull moment_instance); }
  namespace MomentNvr   { Result momentNvrInit  (MomentInstance * mt_nonnull moment_instance); }
  /*
  #if !defined (LIBMARY_PLATFORM_ANDROID) && !defined (LIBMARY_PLATFORM_DEFAULT)
    namespace MomentThumb { Result momentThumbInit (MomentInstance * mt_nonnull moment_instance); }
  #endif
  */
#endif


namespace Moment {

Result loadModules (MomentInstance * const mt_nonnull moment_instance,
                    ConstMemory     module_path)
{
  #ifdef MOMENT_STATIC_MODULES
      (void) module_path;

      if (!MomentTest::momentTestInit (moment_instance)) {
          logE_ (_func, "momentTestInit() failed");
          return Result::Failure;
      }

      if (!MomentFile::momentFileInit (moment_instance)) {
          logE_ (_func, "momentFileInit() failed");
          return Result::Failure;
      }

      if (!MomentVod::momentVodInit (moment_instance)) {
          logE_ (_func, "momentVodInit() failed");
          return Result::Failure;
      }

      if (!MomentAuth::momentAuthInit (moment_instance)) {
          logE_ (_func, "momentAuthInit() failed");
          return Result::Failure;
      }

      if (!MomentRtmp::momentRtmpInit (moment_instance)) {
          logE_ (_func, "momentRtmpInit() failed");
          return Result::Failure;
      }

      if (!MomentRtsp::momentRtspInit (moment_instance)) {
          logE_ (_func, "momentRtspInit() failed");
          return Result::Failure;
      }

      if (!MomentNvr::momentNvrInit (moment_instance)) {
          logE_ (_func, "momentNvrInit() failed");
          return Result::Failure;
      }

    /*
    #if !defined (LIBMARY_PLATFORM_ANDROID) && !defined (LIBMARY_PLATFORM_DEFAULT)
      if (!MomentThumb::momentThumbInit (moment_instance)) {
          logE_ (_func, "momentThumbInit() failed");
          return Result::Failure;
      }
    #endif
    */
  #else
    #ifndef LIBMARY_PLATFORM_WIN32
      if (module_path.len() == 0)
          module_path = LIBMOMENT_PREFIX "/moment-1.0";

      logD_ (_func, "module_path: ", module_path);

      StRef<Vfs> const vfs = Vfs::createDefaultLocalVfs (module_path);
      if (!vfs)
          return Result::Failure;

      StRef<Vfs::VfsDirectory> const dir = vfs->openDirectory (ConstMemory());
      if (!dir)
          return Result::Failure;

      StringHash<EmptyBase> loaded_names;

      StRef<String> const mod_gst_name  = makeString (module_path, "/libmoment-gst-1.0");
      StRef<String> const mod_stat_name = makeString (module_path, "/libmoment-stat-1.0");

      bool loading_mod_stat = true;
      for (;;) {
          for (;;) {
              StRef<String> dir_entry;
              if (!dir->getNextEntry (dir_entry))
                  return Result::Failure;
              if (!dir_entry)
                  break;

              StRef<String> const stat_path = makeString (module_path, "/", dir_entry->mem());
              ConstMemory const entry_name = stat_path->mem();

              Vfs::FileStat stat_data;
              if (!vfs->stat (dir_entry->mem(), &stat_data)) {
                  logE_ (_func, "Could not stat ", stat_path);
                  continue;
              }

              // TODO Find rightmost slash, then skip one dot.
              ConstMemory module_name = entry_name;
              {
                  void *dot_ptr = memchr ((void*) entry_name.mem(), '.', entry_name.len());
                  // XXX Dirty.
                  // Skipping the first dot (belongs to "moment-1.0" substring).
                  if (dot_ptr)
                      dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1),
                                        '.',
                                        entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
                  // Skipping the second dot (-1.0 in library version).
                  if (dot_ptr)
                      dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1),
                                        '.',
                                        entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
                #ifdef LIBMARY_PLATFORM_WIN32
                  // TEST: skipping the third dot.
                  // TODO The dots should be skipped from the end of the string!
                  if (dot_ptr)
                      dot_ptr = memchr ((void*) ((Byte const *) dot_ptr + 1),
                                        '.',
                                        entry_name.len() - ((Byte const *) dot_ptr - entry_name.mem()) - 1);
                #endif

                  if (dot_ptr)
                      module_name = entry_name.region (0, (Byte const *) dot_ptr - entry_name.mem());
              }

              if (equal (module_name, mod_gst_name->mem()))
                  continue;

              if (loading_mod_stat) {
                  if (!equal (module_name, mod_stat_name->mem()))
                      continue;
              }

              if (stat_data.file_type == Vfs::FileType::RegularFile &&
                  !loaded_names.lookup (module_name))
              {
                  loaded_names.add (module_name, EmptyBase());

                  logD_ (_func, "loading module ", module_name);

                  if (!loadModule (module_name, moment_instance)) {
                      logE_ (_func, "Could not load module ", module_name, ": ", exc->toString());
                      return Result::Failure;
                  }
              }
          }

          if (loading_mod_stat) {
              loading_mod_stat = false;
              dir->rewind ();
              continue;
          }

          break;
      }

      {
        // Loading mod_gst last, so that it deinitializes first.
        // We count on the fact that M::Informer prepends new subscribers
        // to the beginning of subscriber list, which is hacky, because
        // M::Informer has no explicit guarantees for that.
        //
        // This is important for proper deinitialization. Ideally, the order
        // of module deinitialization should not matter.
        // The process of deinitialization needs extra thought.

          assert (!loaded_names.lookup (mod_gst_name->mem()));
          loaded_names.add (mod_gst_name->mem(), EmptyBase());

          logD_ (_func, "loading module (forced last) ", mod_gst_name);
          if (!loadModule (mod_gst_name->mem(), moment_instance))
              logW_ (_func, "Could not load module ", mod_gst_name, ": ", exc->toString());
      }
    #else /* LIBMARY_PLATFORM_WIN32 */
      {
          // if (!loadModule ("../lib/bin/libmoment-stat-1.0-0.dll"))
          //    logE_ (_func, "Could not load mod_stat (win32)");

          if (!loadModule ("../lib/bin/libmoment-file-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_file (win32)");

          if (!loadModule ("../lib/bin/libmoment-rtsp-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_rtsp (win32)");

          if (!loadModule ("../lib/bin/libmoment-hls-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_hls (win32)");

          if (!loadModule ("../lib/bin/libmoment-rtmp-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_rtmp (win32)");

          if (!loadModule ("../lib/bin/libmoment-gst-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_gst (win32)");

          if (!loadModule ("../lib/bin/libmoment-transcoder-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_transcoder (win32)");

          if (!loadModule ("../lib/bin/libmoment-mychat-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mychat module (win32)");

          if (!loadModule ("../lib/bin/libmoment-test-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mychat module (win32)");

          if (!loadModule ("../lib/bin/libmoment-auth-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_auth (win32)");

          if (!loadModule ("../lib/bin/libmoment-nvr-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load mod_nvr (win32)");

          if (!loadModule ("../lib/bin/libmoment-lectorium-1.0-0.dll", moment_instance))
              logE_ (_func, "Could not load lectorium (win32)");
      }
    #endif
  #endif

    return Result::Success;
}

}

