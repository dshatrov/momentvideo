/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__STAT_COUNTER__H__
#define LIBMARY__STAT_COUNTER__H__


#include <libmary/types.h>
#ifdef LIBMARY_MT_SAFE
  #include <atomic>
#endif

#include <libmary/map.h>


// TODO Global config var which enables stat counters.


namespace M {

class StatCounterEntry
{
  public:
    mt_const Count index;

    typedef Map< StatCounterEntry*,
                 MemberExtractor< StatCounterEntry,
                                  Count,
                                  &StatCounterEntry::index >,
                 DirectComparator<Count> >
            StatCounterMap;

    mt_mutex (StatCounters::mutex) StatCounterMap::Entry map_entry;
    mt_mutex (StatCounters::mutex) bool registered;

    StatCounterEntry (Count const index)
        : index (index),
          registered (false)
    {}

    virtual ~StatCounterEntry ();
};

class StatCounter : public StatCounterEntry
{
  public:
    enum Type 
    {
        t_Uint64,
        t_AtomicInt
    };

  private:
    Type const type;
    ConstMemory const name;
    ConstMemory const section_name;

  public:
    Type        getType () const { return type; }
    ConstMemory getName () const { return name; }
    ConstMemory getSectionName () const { return section_name; }

    StatCounter (Uint64      const index,
                 Type        const type,
                 ConstMemory const name,
                 ConstMemory const section_name)
        : StatCounterEntry (index),
          type (type),
          name (name),
          section_name (section_name)
    {}
};

class StatCounter_Uint64 : public StatCounter
{
  public:
    template <StatCounter_Uint64 *ptr>
    class Unit
    {
      public:
         Unit () { ptr->add (1); }
        ~Unit () { ptr->add ((Uint64) -1); }
    };

  private:
    #ifdef LIBMARY_MT_SAFE
      std::atomic<Uint64> value;
    #else
      Uint64 value;
    #endif

  public:
    void set (Uint64 const new_value)
    {
      #ifdef LIBMARY_MT_SAFE
        value.store (new_value, std::memory_order_relaxed);
      #else
        value = new_value;
      #endif
    }

    void add (Uint64 const a)
    {
      #ifdef LIBMARY_MT_SAFE
        value.fetch_add (a, std::memory_order_relaxed);
      #else
        value += a;
      #endif
    }

    Uint64 get ()
    {
      #ifdef LIBMARY_MT_SAFE
        return value.load (std::memory_order_relaxed);
      #else
        return value;
      #endif
    }

    StatCounter_Uint64 (Count       const index,
                        ConstMemory const name,
                        Uint64      const value = 0,
                        ConstMemory const section_name = ConstMemory())
        : StatCounter (index, t_Uint64, name, section_name),
          value (value)
    {}
};

class StatCounter_AtomicInt : public StatCounter
{
  public:
    template <StatCounter_AtomicInt *ptr>
    class Unit
    {
      public:
         Unit () { ptr->add ( 1); }
        ~Unit () { ptr->add (-1); }
    };

  private:
    #ifdef LIBMARY_MT_SAFE
      std::atomic<int> value;
    #else
      int value;
    #endif

  public:
    void set (int const new_value)
    {
      #ifdef LIBMARY_MT_SAFE
        value.store (new_value, std::memory_order_relaxed);
      #else
        value = new_value;
      #endif
    }

    void add (int const a)
    {
      #ifdef LIBMARY_MT_SAFE
        value.fetch_add (a, std::memory_order_relaxed);
      #else
        value += a;
      #endif
    }

    int get ()
    {
      #ifdef LIBMARY_MT_SAFE
        return value.load (std::memory_order_relaxed);
      #else
        return value;
      #endif
    }

    StatCounter_AtomicInt (Count       const index,
                           ConstMemory const name,
                           int         const value = 0,
                           ConstMemory const section_name = ConstMemory())
        : StatCounter (index, t_AtomicInt, name, section_name),
          value (value)
    {}
};

void registerStatCounter (StatCounter * mt_nonnull counter);
void removeStatCounter   (StatCounter * mt_nonnull counter);

StRef<String> statCountersToString ();

void initStatCounters ();
void releaseStatCounters ();

}


#endif /* LIBMARY__STAT_COUNTER__H__ */

