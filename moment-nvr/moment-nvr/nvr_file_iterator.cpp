/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment-nvr/nvr_file_iterator.h>


namespace MomentNvr {

static LogGroup libMary_logGroup_file_iter ("mod_nvr.file_iter", LogLevel::D);

StRef<String>
NvrFileIterator::makePathForDepth (ConstMemory   const stream_name,
                                   unsigned      const depth,
                                   unsigned    * const mt_nonnull pos)
{
    StRef<String> str;

    Format fmt;
    fmt.min_digits = 2;

    switch (depth) {
        case 5: str = makeString (fmt, stream_name,"/",pos[0],"/",pos[1],"/",pos[2],"/",pos[3],"/",pos[4]); break;
        case 4: str = makeString (fmt, stream_name,"/",pos[0],"/",pos[1],"/",pos[2],"/",pos[3]); break;
        case 3: str = makeString (fmt, stream_name,"/",pos[0],"/",pos[1],"/",pos[2]); break;
        case 2: str = makeString (fmt, stream_name,"/",pos[0],"/",pos[1]); break;
        case 1: str = makeString (fmt, stream_name,"/",pos[0]); break;
        default:
            unreachable ();
    }
    return str;
}

StRef<String>
NvrFileIterator::getNext_rec (Vfs::VfsDirectory * const mt_nonnull parent_dir,
                              ConstMemory         const parent_dir_name,
                              unsigned            const depth,
                              bool                const parent_pos_match)
{
    unsigned const target = (parent_pos_match ? cur_pos [depth] : 0);

    logD (file_iter, _func, "depth: ", depth, ", parent_dir_name: ", parent_dir_name, ", target: ", target, " pp_match: ", parent_pos_match);

    Format fmt;
    fmt.min_digits = 2;

#if 0
// TODO Is this step completely unnecessary?
    if (depth < 5) {
        StRef<String> const dir_name = makeString (parent_dir_name, "/", fmt, target);
        StRef<Vfs::VfsDirectory> const dir = vfs->openDirectory (dir_name->mem());
        if (dir) {
            logD (file_iter, _func, "descending into ", dir_name);
            if (StRef<String> const str = getNext_rec (dir, dir_name->mem(), depth + 1)) {
                logD (file_iter, _func, "result: ", str);
                return str;
            }
        }
    }
#endif

    AvlTree<unsigned> subdir_tree;
    AvlTree<unsigned> vdat_tree;
    {
        StRef<String> entry_name;
        while (parent_dir->getNextEntry (entry_name) && entry_name) {
            bool is_vdat = false;
            ConstMemory number_mem;
            if (stringHasSuffix (entry_name->mem(), ".vdat", &number_mem)) {
                is_vdat = true;
            } else {
                number_mem = entry_name->mem();
            }

            Uint32 number = 0;
            if (strToUint32_safe (number_mem, &number, 10 /* base */)) {
                logD (file_iter, _func, "is_vdat: ", is_vdat, ", number_mem: ", number_mem);
                if (is_vdat)
                    vdat_tree.add (number);
                else
                    subdir_tree.add (number);
            }
        }
    }

    if (vdat_tree.isEmpty() && depth < 5) {
        logD (file_iter, _func, "walking subdir_tree");

        AvlTree<unsigned>::bl_iterator iter (subdir_tree);
        while (!iter.done()) {
            unsigned const number = iter.next ()->value;
            if (number < target)
                continue;

            StRef<String> const dir_name = makeString (parent_dir_name, "/", fmt, number);
            StRef<Vfs::VfsDirectory> const dir = vfs->openDirectory (dir_name->mem());
            if (dir) {
                if (StRef<String> const str =
                            getNext_rec (dir, dir_name->mem(), depth + 1, (target == number && parent_pos_match)))
                {
                    cur_pos [depth] = number;
                    return str;
                }
            }
        }
    } else {
        logD (file_iter, _func, "walking vdat_tree");

        AvlTree<unsigned>::bl_iterator iter (vdat_tree);
        unsigned prv_number = 0;
        bool got_prv_number = false;
        bool got_number = false;
        unsigned number = 0;
        while (!iter.done()) {
            AvlTree<unsigned>::Node* const node = iter.next ();
            number = node->value;
            logD (file_iter, _func, "number ", number);

            prv_number = number;
            got_prv_number = true;

            if (!got_first) {
                logD (file_iter, _func, "!got_first");

                AvlTree<unsigned>::bl_iterator next_iter (node);
                assert (!next_iter.done());
                next_iter.next ();
                if (!next_iter.done()) {
                    logD (file_iter, _func, "!next_iter.done()");

                    AvlTree<unsigned>::Node* const next_node = next_iter.next ();
                    logD (file_iter, _func, "target ", target, ", next_node->value ", next_node->value);
                    if (target < next_node->value) {
                        logD (file_iter, _func, "matched by the next node");
                        got_number = true;
                    }
                } else {
                    logD (file_iter, _func, "next_iter.done()");
                    got_number = true;
                }
            }

            if (!got_number) {
                if (number < target) {
                    logD (file_iter, _func, "less than target: ", number, " < ", target);
                    continue;
                } else
                if (number == target) {
                    if (got_first && parent_pos_match) {
                        logD (file_iter, _func, "number == target; got_first && parent_pos_match");
                        continue;
                    }
                }

                got_number = true;
            }

            break;
        }

        if (!got_number && !got_first && parent_pos_match && got_prv_number) {
            number = prv_number;
            got_number = true;
        }

        if (got_number) {
            logD (file_iter, _func, "match: ", number);

            got_first = true;
            cur_pos [depth] = number;

            StRef<String> const filename = makeString (parent_dir_name, "/", fmt, number);
            logD (file_iter, _func, "result: ", filename);
            return filename;
        }
    }

    return NULL;
}

StRef<String>
NvrFileIterator::getNext ()
{
    logD (file_iter, _func_);

    StRef<Vfs::VfsDirectory> const dir = vfs->openDirectory (stream_name->mem());
    if (!dir) {
        logD_ (_func, "vfs->openDirectory() failed: ", stream_name->mem(), ": ", exc->toString());
        return NULL;
    }

    StRef<String> const filename = getNext_rec (dir, stream_name->mem(), 0, true /* parent_pos_match */);
//    logD_ (_func, "filename: ", filename);
    return filename;
}

void
NvrFileIterator::doSetCurPos (Time const start_unixtime_sec)
{
    struct tm tm;
    if (!unixtimeToStructTm (start_unixtime_sec, &tm)) {
        logE_ (_this_func, "unixtimeToStructTm() failed");
        memset (cur_pos, 0, sizeof (cur_pos));
        return;
    }

    cur_pos [0] = tm.tm_year + 1900;
    cur_pos [1] = tm.tm_mon + 1;
    cur_pos [2] = tm.tm_mday;
    cur_pos [3] = tm.tm_hour;
    cur_pos [4] = tm.tm_min;
    cur_pos [5] = tm.tm_sec;

    logD (file_iter, _this_func, "cur_pos (", start_unixtime_sec,"): ", makePathForDepth ("", 5, cur_pos));
}

void
NvrFileIterator::reset (Time const start_unixtime_sec)
{
    logD (file_iter, _func_);
    doSetCurPos (start_unixtime_sec);
    got_first = false;
}

void
NvrFileIterator::init (Vfs         * const mt_nonnull vfs,
                       ConstMemory   const stream_name,
                       Time          const start_unixtime_sec)
{
    logD (file_iter, _this_func,
          "stream_name \"", stream_name, "\", "
          "start_unixtime_sec ", start_unixtime_sec);

    this->vfs = vfs;
    this->stream_name = st_grab (new (std::nothrow) String (stream_name));
    doSetCurPos (start_unixtime_sec);
}

}

