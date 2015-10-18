/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__NAMESPACE_CONTAINER__H__
#define LIBMARY__NAMESPACE_CONTAINER__H__


#include <libmary/string_hash.h>


namespace M {

template <class T>
mt_unsafe class NamespaceContainer
{
  public:
    struct Namespace;
    class Entry;

    typedef StringHash<Namespace> NamespaceHash;
    typedef StringHash<Entry> EntryHash;

    struct Namespace : public StReferenced
    {
        Namespace *parent_nsp;
        typename NamespaceHash::EntryKey namespace_hash_key;

        NamespaceHash namespace_hash;
        EntryHash entry_hash;

        Namespace ()
            : parent_nsp (NULL)
        {}
    };

    class Entry
    {
        friend class NamespaceContainer<T>;

        Namespace *parent_nsp;
        typename EntryHash::EntryKey entry_hash_key;

      public:
        T data;
    };

    Namespace root_namespace;

  private:
    // @ret_path_tail should contain original path.
    Entry* getEntry_rec (ConstMemory   const path,
                         ConstMemory * const mt_nonnull ret_path_tail,
                         Namespace   * const mt_nonnull nsp)
    {
        Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
        ConstMemory next_name;
        if (delim) {
            next_name = path.region (0, delim - path.mem());
            typename NamespaceHash::EntryKey const next_nsp_key = nsp->namespace_hash.lookup (next_name);
            if (next_nsp_key) {
                Namespace * const next_nsp = next_nsp_key.getDataPtr();
                Entry * const entry = getEntry_rec (path.region (delim - path.mem() + 1),
                                                    ret_path_tail,
                                                    next_nsp);
                if (entry)
                    return entry;
            }
        } else {
            next_name = path;
        }

        typename EntryHash::EntryKey const entry_key = nsp->entry_hash.lookup (next_name);
        if (!entry_key)
            return NULL;

        *ret_path_tail = (*ret_path_tail).region ((next_name.mem() + next_name.len()) - (*ret_path_tail).mem());
        return entry_key.getDataPtr();
    }

  public:
    Entry* getEntry (ConstMemory  path,
                     ConstMemory * const mt_nonnull ret_path_tail)
    {
        if (path.len() > 0 && path.mem() [0] == '/')
            path = path.region (1);

        *ret_path_tail = path;
        return getEntry_rec (path, ret_path_tail, &root_namespace);
    }

  private:
    Entry* addEntry_rec (ConstMemory  path,
                         Namespace   * const nsp)
    {
        if (path.len() > 0 && path.mem() [0] == '/')
            path = path.region (1);

        Byte const *delim = (Byte const *) memchr (path.mem(), '/', path.len());
        if (!delim) {
            Entry *entry;
            {
                typename EntryHash::EntryKey entry_key = nsp->entry_hash.lookup (path);
                if (!entry_key) {
                    entry_key = nsp->entry_hash.addEmpty (path);
                    entry = entry_key.getDataPtr();
                    entry->parent_nsp = nsp;
                    entry->entry_hash_key = entry_key;
                } else {
                    entry = entry_key.getDataPtr();
                }
            }

            return entry;
        }

        ConstMemory const next_nsp_name = path.region (0, delim - path.mem());
        typename NamespaceHash::EntryKey const next_nsp_key =
                nsp->namespace_hash.lookup (next_nsp_name);
        Namespace *next_nsp;
        if (next_nsp_key) {
            next_nsp = next_nsp_key.getDataPtr();
        } else {
            typename NamespaceHash::EntryKey const namespace_hash_key = nsp->namespace_hash.addEmpty (next_nsp_name);
            Namespace * const new_nsp = namespace_hash_key.getDataPtr();
            new_nsp->parent_nsp = nsp;
            new_nsp->namespace_hash_key = namespace_hash_key;
            next_nsp = new_nsp;
        }

        return addEntry_rec (path.region (delim - path.mem() + 1), next_nsp);
    }

  public:
    Entry* addEntry (ConstMemory const path)
    {
        return addEntry_rec (path, &root_namespace);
    }

    void removeEntry (Entry * const mt_nonnull entry)
    {
        entry->parent_nsp->entry_hash.remove (entry->entry_hash_key);

        {
            Namespace *nsp = entry->parent_nsp;
            while (nsp && nsp->parent_nsp) {
                if (nsp->entry_hash.isEmpty() &&
                    nsp->namespace_hash.isEmpty())
                {
                    Namespace * const tmp_nsp = nsp;
                    nsp = nsp->parent_nsp;
                    nsp->namespace_hash.remove (tmp_nsp->namespace_hash_key);
                }
            }
        }
    }
};

}


#endif /* LIBMARY__NAMESPACE_CONTAINER__H__ */

