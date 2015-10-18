/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__LOG_ROTATE_STREAM__H__
#define LIBMARY__LOG_ROTATE_STREAM__H__


#include <libmary/intrusive_avl_tree.h>
#include <libmary/state_mutex.h>
#include <libmary/vfs.h>


namespace M {

class LogRotateStream : public OutputStream,
                        public Object
{
  private:
    StateMutex mutex;

    struct EntryDate
    {
        Uint64 idx;
        int    tm_year; // year - 1900
        int    tm_mon;  // month of year (0 - 11)
        int    tm_mday; // day of month (1 - 31)

        bool isSameDay (EntryDate * const date)
        {
            return    date->tm_year == tm_year
                   && date->tm_mon  == tm_mon
                   && date->tm_mday == tm_mday;
        }

        EntryDate ()
            : idx     (0),
              tm_year (0),
              tm_mon  (0),
              tm_mday (0)
        {}
    };

    struct EntryNode : public IntrusiveAvlTree_Node<>
    {
        StRef<String> filename;
        EntryDate     date;
    };

    typedef IntrusiveAvlTree< EntryNode,
                              MemberExtractor< EntryNode,
                                               EntryDate,
                                               &EntryNode::date,
                                               Uint64,
                                               MemberExtractor< EntryDate,
                                                                Uint64,
                                                                &EntryDate::idx > >,
                              DirectComparator<Uint64>,
                              IntrusiveAvlTree_name,
                              DeleteAction<EntryNode> >
            EntryNodeTree;

    mt_const String log_dir;
    mt_const String log_filename_prefix;
    mt_const String log_filename_suffix;

    mt_const Uint64 max_days;
    mt_const Uint64 max_total_size;
    mt_const Uint64 max_file_size;

    mt_const StRef<Vfs> vfs;

    mt_mutex (mutex) Ref<OutputStream> cur_stream;
    mt_mutex (mutex) Uint64            cur_file_size;
    mt_mutex (mutex) EntryDate         cur_date;

    mt_mutex (mutex) bool first_write_after_flush;
    mt_mutex (mutex) bool rotating_now;

    static Result parseEntryDate (ConstMemory  mem,
                                  ConstMemory  prefix,
                                  ConstMemory  suffix,
                                  EntryDate   *ret_date);

    StRef<String> makeLogFilename (EntryDate const *date);

    mt_mutex (mutex) mt_throws Ref<OutputStream> openLogFile (ConstMemory filename,
                                                              bool        add_newline);

    mt_unlocks_locks (mutex) mt_throws Result rotateLogIfNecessary ();

  public:
    static ConstMemory getDefaultLogFilenamePrefix () { return ConstMemory (""); }
    static ConstMemory getDefaultLogFilenameSuffix () { return ConstMemory (".log"); }
    static Uint64      getDefaultMaxDays           () { return  365; }
    static Uint64      getDefaultMaxTotalSize      () { return 1024 * (1 << 20); }
    static Uint64      getDefaultMaxFileSize       () { return   16 * (1 << 20); }

    mt_iface (OutputStream)
      mt_throws Result write (ConstMemory  mem,
                              Size        *ret_nwritten);

      mt_throws Result flush ();
    mt_iface_end

    mt_const void init (ConstMemory log_dir_mem,
                        ConstMemory log_filename_prefix_mem = getDefaultLogFilenamePrefix(),
                        ConstMemory log_filename_suffix_mem = getDefaultLogFilenameSuffix(),
                        Uint64      max_days                = getDefaultMaxDays(),
                        Uint64      max_total_size          = getDefaultMaxTotalSize(),
                        Uint64      max_file_size           = getDefaultMaxFileSize());

    LogRotateStream (EmbedContainer *embed_container);
};

}


#endif /* LIBMARY__LOG_ROTATE_STREAM__H__ */

