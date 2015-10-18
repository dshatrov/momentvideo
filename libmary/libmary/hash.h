/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__HASH__H__
#define LIBMARY__HASH__H__


#include <libmary/types.h>
#include <libmary/intrusive_list.h>
#include <libmary/intrusive_avl_tree.h>


namespace M {

class DefaultStringHasher
{
  public:
    static Uint32 hash (ConstMemory const &mem)
    {
        Uint32 result = 5381;

        for (Size i = 0; i < mem.len(); ++i)
            result = ((result << 5) + result) ^ mem.mem() [i];

        return result;
    }
};

class Hash_Default;

template <class HashName = Hash_Default>
class HashEntry : public IntrusiveAvlTree_Node<HashName>,
                  public IntrusiveListElement<HashName>
{
    template <class T,
              class KeyType,
              class Extractor,
              class Comparator,
              class Hasher,
              class HashName_>
            friend class Hash_anybase;

    template <class T,
              class KeyType,
              class Extractor,
              class Comparator,
              class Hasher,
              class HashName_,
              class Base>
            friend class Hash;

  private:
    Uint32 unrolled_hash;
};

template < class T,
           class KeyType,
           class Extractor  = DirectExtractor<T>,
           class Comparator = DirectComparator<T>,
           class Hasher     = DefaultStringHasher,
           class HashName   = Hash_Default>
class Hash_anybase
{
  private:
    class Cell
    {
      public:
        IntrusiveAvlTree< T, Extractor, Comparator, HashName > tree;
    };

    bool growing;

    Cell *hash_table;
    Size  hash_size;

  public:
    typedef IntrusiveList<T, HashName> NodeList;

  private:
    NodeList node_list;

  public:
    bool isEmpty () const { return node_list.isEmpty(); }

    NodeList const * getNodeList () const { return &node_list; }

    void add (T * const entry)
    {
        Uint32 const unrolled_hash = Hasher::hash (Extractor::getValue (entry));
        Uint32 const hash = unrolled_hash % hash_size;

        entry->unrolled_hash = unrolled_hash;
        hash_table [hash].tree.add (entry);

        node_list.append (entry);

        // TODO if (growing) ... then grow.
    }

    void remove (T * const entry)
    {
        hash_table [entry->unrolled_hash % hash_size].tree.remove (entry);
        node_list.remove (entry);
    }

    void clear ()
    {
        node_list.clear ();

        delete[] hash_table;
        hash_table = new (std::nothrow) Cell [hash_size];
        assert (hash_table);
    }

    template <class C>
    T* lookup (C key)
    {
        Uint32 const unrolled_hash = Hasher::hash (key);
        Uint32 const hash = unrolled_hash % hash_size;

        return hash_table [hash].tree.lookup (key);
    }

    Hash_anybase (Size const initial_hash_size,
                  bool const growing)
        : growing (growing),
          hash_size (initial_hash_size)
    {
        hash_table = new (std::nothrow) Cell [hash_size];
        assert (hash_table);
    }

    ~Hash_anybase ()
    {
        delete[] hash_table;
    }


  // ________________________________ iterator _________________________________

    class iterator
    {
      private:
        typename NodeList::iterator node_iter;

      public:
        iterator (Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName > &hash)
                : node_iter (hash.node_list) {}
        iterator () {}

        bool operator == (iterator const &iter) const { return node_iter == iter.node_iter; }
        bool operator != (iterator const &iter) const { return node_iter != iter.node_iter; }

        bool done () const { return node_iter.done(); }
        T* next () { return node_iter.next(); }
    };


  // __________________________________ iter ___________________________________

    class iter
    {
        friend class Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName >;

      private:
        typename NodeList::iter node_iter;

      public:
        iter (Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName > &hash)
            : node_iter (hash.node_list) {}
        iter () {}

        // Methods for C API binding.
        void *getAsVoidPtr () const { return node_iter.getAsVoidPtr (); }
        static iter fromVoidPtr (void *ptr)
        {
            iter it;
            it.node_iter = NodeList::iter::fromVoidPtr (ptr);
            return it;
        }
    };

    void iter_begin (iter &iter) const
        { node_list.iter_begin (iter.node_iter); }

    T* iter_next (iter &iter) const
        { return node_list.iter_next (iter.node_iter); }

    bool iter_done (iter &iter) const
        { return node_list.iter_done (iter.node_iter); }

  // ___________________________________________________________________________

};

template < class T,
           class KeyType,
           class Extractor  = DirectExtractor<T>,
           class Comparator = DirectComparator<T>,
           class Hasher     = DefaultStringHasher,
           class HashName   = Hash_Default,
           class Base       = EmptyBase>
class Hash : public Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName >,
             public Base
{
  public:
    Hash (Size const initial_hash_size = 16,
          bool const growing = true)
        : Hash_anybase< T, KeyType, Extractor, Comparator, Hasher, HashName > (initial_hash_size, growing)
    {}
};

}


#endif /* LIBMARY__HASH__H__ */

