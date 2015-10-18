/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/stat_counter.h>


namespace M {

typedef StatCounterEntry::StatCounterMap StatCounterMap;

struct StatCounters
{
    Mutex mutex;
    mt_mutex (mutex) StatCounterMap counter_map;
};

static StatCounters *counters = NULL;

StatCounterEntry::~StatCounterEntry ()
{
    counters->mutex.lock ();

    if (registered)
        counters->counter_map.remove (map_entry);

    counters->mutex.unlock ();
}

void registerStatCounter (StatCounter * const mt_nonnull counter)
{
    counters->mutex.lock ();
    if (counter->registered) {
        counters->mutex.unlock ();
        return;
    }

    counter->registered = true;
    counter->map_entry = counters->counter_map.add (counter);
    counters->mutex.unlock ();
}

void removeStatCounter (StatCounter * const mt_nonnull counter)
{
    counters->mutex.lock ();
    counters->counter_map.remove (counter->map_entry);
    counter->registered = false;
    counters->mutex.unlock ();
}

StRef<String> statCountersToString ()
{
    StRef<String> str = makeString ("stat counters:\n");
    counters->mutex.lock ();

    {
        Count prv_idx = 0;
        StatCounterMap::iterator iter (counters->counter_map);
        while (!iter.done()) {
            StatCounter * const counter = static_cast <StatCounter*> (iter.next ().getData());

            if (counter->index - prv_idx > 1) {
                if (counter->getSectionName().len())
                    str = makeString (str, "--- ", counter->getSectionName(), " ---\n");
                else
                    str = makeString (str, "---\n");
            }

            switch (counter->getType()) {
                case StatCounter::t_Uint64: {
                    StatCounter_Uint64 * const counter_uint64 = static_cast <StatCounter_Uint64*> (counter);
                    Uint64 const value = counter_uint64->get ();
                    str = makeString (str, counter->getName(), ": ", value, "\n");
                } break;
                case StatCounter::t_AtomicInt: {
                    StatCounter_AtomicInt * const counter_aint = static_cast <StatCounter_AtomicInt*> (counter);
                    int const value = counter_aint->get ();
                    str = makeString (str, counter->getName(), ": ", value, "\n");
                } break;
            }

            prv_idx = counter->index;
        }
    }

    counters->mutex.unlock ();
    return str;
}

void initStatCounters ()
{
    counters = new (std::nothrow) StatCounters;
    assert (counters);
}

void releaseStatCounters ()
{
  // No-op until careful module unloading is implemented.
}

}

