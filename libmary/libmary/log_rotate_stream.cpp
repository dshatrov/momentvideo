/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/util_str.h>
#include <libmary/log.h>

#include <libmary/log_rotate_stream.h>


// NOTE: Be careful with calling log*() functions here. LogRotateStream is a part of logging chain.


namespace M {

static Result parseNumber (bool            const last,
                           unsigned char   const delimiter,
                           ConstMemory   * const mem,
                           Uint64        * const mt_nonnull ret_number)
{
    unsigned char const *buf = mem->buf();
    size_t len = mem->len();

    *ret_number = 0;

    if (len < 1 || buf [0] != delimiter) {
        return Result::Failure;
    }
    ++buf;
    --len;

    Uint64 value;
    Byte const *endptr;
    if (!strToUint64 (ConstMemory (buf, len), &value, &endptr, /*base=*/ 10))
        return Result::Failure;

    if (last) {
        if (endptr != buf + len)
            return Result::Failure;
    }

    *ret_number = value;
    *mem = ConstMemory (endptr, len - (endptr - buf));
    return Result::Success;
}

Result
LogRotateStream::parseEntryDate (ConstMemory         mem,
                                 ConstMemory   const prefix,
                                 ConstMemory   const suffix,
                                 EntryDate   * const ret_date)
{
    if (!stringHasPrefix (mem, prefix)) {
        return Result::Failure;
    }
    mem = mem.region (prefix.len());

    Uint64 idx;
    if (!parseNumber (/*last=*/ false, '_', &mem, &idx)) {
        return Result::Failure;
    }
    ret_date->idx = idx;

    Uint64 year;
    if (!parseNumber (/*last=*/ false, '_', &mem, &year))
        return Result::Failure;

    Uint64 month;
    if (!parseNumber (/*last=*/ false, '-', &mem, &month))
        return Result::Failure;

    Uint64 day;
    if (!parseNumber (/*last=*/ false, '-', &mem, &day))
        return Result::Failure;

    if (!equal (mem, suffix))
        return Result::Failure;

    if (year < 1900) {
        return Result::Failure;
    }
    ret_date->tm_year = year - 1900;

    if (month < 1) {
        return Result::Failure;
    }
    ret_date->tm_mon = month - 1;

    if (day < 1 || day > 31) {
        return Result::Failure;
    }
    ret_date->tm_mday = day;

    return Result::Success;
}

StRef<String>
LogRotateStream::makeLogFilename (EntryDate const * const date)
{
    Format idx_fmt  (/*num_base=*/   10,
                     /*min_digits=*/  4,
                     /*precision=*/  (unsigned) -1);

    Format year_fmt (/*num_base=*/   10,
                     /*min_digits=*/  4,
                     /*precision=*/  (unsigned) -1);

    Format mmdd_fmt (/*num_base=*/   10,
                     /*min_digits=*/  2,
                     /*precision=*/  (unsigned) -1);

    return makeString (log_filename_prefix,
                       "_", idx_fmt,  date->idx,
                       "_", year_fmt, date->tm_year + 1900,
                       "-", mmdd_fmt, date->tm_mon + 1,
                       "-",           date->tm_mday,
                       log_filename_suffix);
}

mt_mutex (mutex) mt_throws Ref<OutputStream>
LogRotateStream::openLogFile (ConstMemory const filename,
                              bool        const add_newline)
{
    StRef<Vfs::VfsFile> const vfs_file = mt_throws vfs->openFile (filename, FileOpenFlags::Create, FileAccessMode::WriteOnly);
    if (!vfs_file) {
        logE_ (_this_func, "openFile(\"", filename, "\" failed: ", exc->toString());
        return NULL;
    }

    if (! mt_throws vfs_file->getFile()->seek (/*offset=*/ 0, SeekOrigin::End)) {
        logE_ (_this_func, "seek(\"", filename, "\" failed: ", exc->toString());
        return NULL;
    }

    if (add_newline) {
        if (! mt_throws vfs_file->getFile()->writeFull ("\n", /*ret_nwritten=*/ NULL)) {
            logE_ (_this_func, "writeFull(\"", filename, "\" failed: ", exc->toString());
            return NULL;
        }
    }

    return vfs_file->getFile();
}

mt_unlocks_locks (mutex) mt_throws Result
LogRotateStream::rotateLogIfNecessary ()
{
    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();

    if (cur_stream
        && (cur_file_size < max_file_size)
        && (   cur_date.tm_year == tlocal->localtime.tm_year
            && cur_date.tm_mon  == tlocal->localtime.tm_mon
            && cur_date.tm_mday == tlocal->localtime.tm_mday))
    {
      // no need to open a new log file
        return Result::Success;
    }

    if (rotating_now) {
      return Result::Success;
    }
    rotating_now = true;

    EntryDate new_date = cur_date;

    mutex.unlock ();

  // opening a new log file

    EntryNodeTree entry_tree;

    StRef<String>  latest_entry;
    EntryNode     *latest_node = NULL;

    StRef<Vfs::VfsDirectory> const dir = vfs->openDirectory ("/");
    for (;;) {
        StRef<String> entry;
        if (!dir->getNextEntry (entry)) {
            logW_ (_this_func, "dir->getNextEntry() failed: ", exc->toString());
            break;
        }

        if (!entry) {
            break;
        }
        logD_ (_this_func, "entry: \"", entry, "\"");

        if (entry->len() == 0 || entry->buf() [0] == '.')
            continue;

        EntryDate date;
        if (!parseEntryDate (entry, log_filename_prefix, log_filename_suffix, &date)) {
            logD_ (_this_func, "ignoring file \"", entry, "\"");
            continue;
        }

        EntryNode * const node = new (std::nothrow) EntryNode;
        assert (node);
        node->filename = entry;
        node->date = date;
        entry_tree.add (node);

        if (!latest_node
            || date.idx >= latest_node->date.idx)
        {
            latest_entry = entry;
            latest_node  = node;
        }
    }

    Ref<OutputStream> new_stream;
    Ref<ExceptionBuffer> open_exc_buf;
    bool file_opened = false;
    Uint64 new_file_size = 0;

    if (latest_node
        && (   latest_node->date.tm_year == tlocal->localtime.tm_year
            && latest_node->date.tm_mon  == tlocal->localtime.tm_mon
            && latest_node->date.tm_mday == tlocal->localtime.tm_mday))
    {
        assert (String::len (latest_entry));

        FileStat file_stat;
        if (! mt_throws vfs->stat (latest_entry, &file_stat)) {
            logE_ (_this_func, "stat(\"", latest_entry, "\") failed: ", exc->toString());
        } else {
            if (file_stat.size < max_file_size) {
                new_stream = mt_throws openLogFile (latest_entry, /*add_newline=*/ true);
                if (!new_stream) {
                    open_exc_buf = exc_swap ();
                } else {
                    new_file_size = file_stat.size;
                    new_date = latest_node->date;

                    file_opened = true;
                }
            }
        }
    }

    if (!file_opened) {
        if (latest_node)
            new_date = latest_node->date;

        ++new_date.idx;
        new_date.tm_year = tlocal->localtime.tm_year;
        new_date.tm_mon  = tlocal->localtime.tm_mon;
        new_date.tm_mday = tlocal->localtime.tm_mday;

        new_stream = mt_throws openLogFile (makeLogFilename (&new_date), /*add_newline=*/ false);
        if (!new_stream)
            open_exc_buf = exc_swap ();

        new_file_size = 0;

        file_opened = true;
    }

    // Keeping 'max_days' most recent entries by removing them from entry_tree.
    {
        EntryNodeTree::BottomRightIterator iter (entry_tree);

        Uint64 num_days = 1;
        Uint64 total_size = 0;

        EntryDate prv_date;
        bool got_prv_date = false;

        while (!iter.done()) {
            EntryNode * const node = iter.next ();

            if (got_prv_date
                && !prv_date.isSameDay (&node->date))
            {
                ++num_days;
            }
            prv_date = node->date;
            got_prv_date = true;

            if (num_days > max_days)
                break;

            FileStat file_stat;
            if (! mt_throws vfs->stat (node->filename, &file_stat)) {
                logE_ (_this_func, "stat(\"", node->filename, "\") failed: ", exc->toString());
            } else {
                total_size += file_stat.size;
            }

            if (total_size > max_total_size)
                break;

            entry_tree.remove (node);
            // Note that iter state remains valid, because it's pointing
            // to the righmost node.
        }
    }

    {
        EntryNodeTree::SafeDeleteIterator iter (entry_tree);
        while (!iter.done()) {
            EntryNode * const node = iter.next ();

            if (! mt_throws vfs->removeFile (node->filename))
                logE_ (_this_func, "removeFile(\"", node->filename, "\") failed: ", exc->toString());

            delete node;
        }
        entry_tree.clearNoAction ();
    }

    if (!new_stream) {
      exc_set_noref (open_exc_buf);
      open_exc_buf.setNoUnref (NULL);
    }

    mutex.lock ();

    cur_stream    = new_stream;
    cur_file_size = new_file_size;
    cur_date      = new_date;

    rotating_now = false;
    return cur_stream ? Result::Success : Result::Failure;
}

mt_throws Result
LogRotateStream::write (ConstMemory   const mem,
                        Size        * const ret_nwritten)
{
    if (ret_nwritten)
        *ret_nwritten = 0;

    mutex.lock ();
    if (!cur_stream || first_write_after_flush) {
        if (! mt_throws mt_unlocks_locks (mutex) rotateLogIfNecessary ()) {
            mutex.unlock ();
            return Result::Failure;
        }

        first_write_after_flush = false;
    }
    Ref<OutputStream> const tmp_stream = cur_stream;
    cur_file_size += mem.len();
    mutex.unlock ();

    if (!tmp_stream) {
        exc_throw (InternalException, InternalException::BackendError);
        return Result::Failure;
    }

    return mt_throws tmp_stream->write (mem, ret_nwritten);
}

mt_throws Result
LogRotateStream::flush ()
{
    mutex.lock ();
    if (!cur_stream) {
        if (! mt_throws mt_unlocks_locks (mutex) rotateLogIfNecessary ()) {
            mutex.unlock ();
            return Result::Failure;
        }

        mutex.unlock ();
        return Result::Success;
    }
    Ref<OutputStream> const tmp_stream = cur_stream;

    first_write_after_flush = true;
    mutex.unlock ();

    if (!tmp_stream) {
        exc_throw (InternalException, InternalException::BackendError);
        return Result::Failure;
    }

    Result const flush_res = tmp_stream->flush ();

#if 0
// We now rotate in write() when first_write_after_flush.
    mutex.lock ();
    if (! mt_throws mt_unlocks_locks (mutex) rotateLogIfNecessary ()) {
        mutex.unlock ();
        return Result::Failure;
    }
    mutex.unlock ();
#endif

    if (!flush_res) {
        exc_throw (InternalException, InternalException::BackendError);
    }
    return flush_res;
}

mt_const void
LogRotateStream::init (ConstMemory const log_dir_mem,
                       ConstMemory const log_filename_prefix_mem,
                       ConstMemory const log_filename_suffix_mem,
                       Uint64      const max_days,
                       Uint64      const max_total_size,
                       Uint64      const max_file_size)
{
    log_dir              = log_dir_mem;
    log_filename_prefix  = log_filename_prefix_mem;
    log_filename_suffix  = log_filename_suffix_mem;
    this->max_days       = max_days;
    this->max_total_size = max_total_size;
    this->max_file_size  = max_file_size;

    vfs = Vfs::createDefaultLocalVfs (log_dir_mem);

    mutex.lock ();
    assert (!rotating_now);
    // Opening 'cur_stream' to avoid races with NULL 'cur_stream' because of
    // 'rotating_now' flag.
    rotateLogIfNecessary ();
    mutex.unlock ();
}

LogRotateStream::LogRotateStream (EmbedContainer * const embed_container)
    : Object (embed_container),
      log_filename_prefix (getDefaultLogFilenamePrefix()),
      log_filename_suffix (getDefaultLogFilenameSuffix()),
      max_days            (getDefaultMaxDays()),
      max_total_size      (getDefaultMaxTotalSize()),
      max_file_size       (getDefaultMaxFileSize()),
      cur_file_size       (0),
      first_write_after_flush (false),
      rotating_now        (false)
{
}

}

