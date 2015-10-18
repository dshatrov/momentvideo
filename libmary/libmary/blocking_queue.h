/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__BLOCKING_QUEUE__H__
#define LIBMARY__BLOCKING_QUEUE__H__


#include <libmary/list.h>
#include <libmary/fast_cond.h>
#include <libmary/referenced.h>


namespace M {

class BlockingQueue
{
  private:
    Mutex    mutex;
    FastCond fast_cond;

    List< Ref<Referenced> > item_list;

  public:
    void clear ()
    {
        List< Ref<Referenced> > tmp_list;

        mutex.lock ();
        tmp_list.steal (&item_list,
                        item_list.getFirstElement(),
                        item_list.getLastElement(),
                        tmp_list.getLastElement(),
                        GenericList::StealAppend);
        mutex.unlock ();
    }

    void enqueue (Referenced * const item)
    {
        mutex.lock ();
        item_list.prepend (item);
        fast_cond.signalAtLeastOne ();
        mutex.unlock ();
    }

    template <class T>
    void enqueue (Ref<T> const &&item)
    {
        mutex.lock ();
        item_list.prependEmpty ()->data = item;
        fast_cond.signalAtLeastOne ();
        mutex.unlock ();
    }

    Ref<Referenced> dequeue ()
    {
        mutex.lock ();
        while (item_list.isEmpty()) {
            fast_cond.waitOrSpuriousWakeup (&mutex);
        }

        Ref<Referenced> const item = std::move (item_list.getLast());
        item_list.remove (item_list.getLastElement());
        mutex.unlock ();

        return item;
    }

    Count getNumItems ()
    {
        mutex.lock ();
        Count const num_items = item_list.getNumElements();
        mutex.unlock ();
        return num_items;
    }
};

}


#endif /* LIBMARY__BLOCKING_QUEUE__H__ */

