/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__INFORMER__H__
#define LIBMARY__INFORMER__H__


#include <libmary/types.h>
#include <libmary/state_mutex.h>
#include <libmary/object.h>
#include <libmary/virt_ref.h>
#include <libmary/cb.h>
#include <libmary/intrusive_list.h>


namespace M {

// This class should not be used from outside.
class GenericInformer
{
  protected:
    union CallbackPtr
    {
        void *obj;
        VoidFunction func;

        CallbackPtr (void * const obj) : obj (obj) {}
        CallbackPtr (VoidFunction const func) : func (func) {}
    };

    typedef void (*ProxyInformCallback) (CallbackPtr   cb_ptr,
                                         void         *cb_data,
                                         VoidFunction  inform_cb,
                                         void         *inform_data);

    class SubscriptionList_name;
    class SubscriptionInvalidationList_name;

    class Subscription : public IntrusiveListElement<SubscriptionList_name>,
                         public IntrusiveListElement<SubscriptionInvalidationList_name>
    {
      public:
        bool valid;

        GenericInformer *informer;
        bool oneshot;
        Object::DeletionSubscriptionKey del_sbn;

        CallbackPtr cb_ptr;
        void *cb_data;
        WeakRef<Object> weak_ref;

        VirtRef ref_data;

        Subscription (CallbackPtr      const cb_ptr,
                      void           * const cb_data,
                      VirtReferenced * const ref_data,
                      Object         * const guard_obj)
            : cb_ptr (cb_ptr),
              cb_data (cb_data),
              weak_ref (guard_obj),
              ref_data (ref_data)
        {}
    };

    typedef IntrusiveList<Subscription, SubscriptionList_name> SubscriptionList;
    typedef IntrusiveList<Subscription, SubscriptionInvalidationList_name> SubscriptionInvalidationList;

  public:
    class SubscriptionKey
    {
        friend class GenericInformer;
      private:
        Subscription *sbn;
      public:
        operator bool () const { return sbn; }
        SubscriptionKey () : sbn (NULL) {}
        SubscriptionKey (Subscription * const sbn) : sbn (sbn) {}

        // Methods for C API binding.
        void *getAsVoidPtr () const { return static_cast <void*> (sbn); }
        static SubscriptionKey fromVoidPtr (void *ptr) {
                return SubscriptionKey (static_cast <Subscription*> (ptr)); }
   };

  protected:
    Object     * const outer_object;
    StateMutex * const mutex;

    mt_mutex (mutex) SubscriptionList sbn_list;
    mt_mutex (mutex) SubscriptionInvalidationList sbn_invalidation_list;
    mt_mutex (mutex) Count traversing;

    mt_mutex (mutex) void releaseSubscription (Subscription *sbn);

    static void subscriberDeletionCallback (void *_sbn);

    mt_async void informAll (ProxyInformCallback  mt_nonnull proxy_inform_cb,
                             VoidFunction         inform_cb,
                             void                *inform_cb_data);

    mt_async mt_unlocks_locks (mutex) void informAll_locked (ProxyInformCallback  mt_nonnull proxy_inform_cb,
                                                             VoidFunction         inform_cb,
                                                             void                *inform_cb_data);

    SubscriptionKey subscribeVoid (CallbackPtr     cb_ptr,
                                   void           *cb_data,
                                   VirtReferenced *ref_data,
                                   Object         *guard_obj);

    mt_mutex (mutex) SubscriptionKey subscribeVoid_locked (CallbackPtr     cb_ptr,
                                                           void           *cb_data,
                                                           VirtReferenced *ref_data,
                                                           Object         *guard_obj);

  public:
    mt_mutex (mutex) bool gotSubscriptions_locked ()
        { return !sbn_list.isEmpty(); }

    void unsubscribe (SubscriptionKey sbn_key);

    mt_mutex (mutex) void unsubscribe_locked (SubscriptionKey sbn_key);

    // In general, if @embed_container is not null, then @mutex should be
    // the state mutex of @embed_container. There may be concious deviations
    // from this rule.
    GenericInformer (Object     * const mt_nonnull outer_object,
                     StateMutex * const mutex)
        : outer_object (outer_object),
          mutex        (mutex),
          traversing   (0)
    {}

    ~GenericInformer ();
};

// Informer for structs with callbacks.
template <class T>
class Informer_ : public GenericInformer
{
  public:
    typedef void (*InformCallback) (T /* TODO const */ *ev_struct,
                                    void *cb_data,
                                    void *inform_data);

  protected:
    static void proxyInformCallback (CallbackPtr      const cb_ptr,
                                     void           * const cb_data,
                                     VoidFunction     const inform_cb,
                                     void           * const inform_data)
        { ((InformCallback) inform_cb) ((T*) cb_ptr.obj, cb_data, inform_data); }

  public:
    mt_async void informAll (InformCallback   const inform_cb,
                             void           * const inform_cb_data)
        { mt_async GenericInformer::informAll (proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data); }

    mt_async mt_unlocks_locks (mutex) void informAll_locked (InformCallback    const inform_cb,
                                                             void            * const inform_cb_data)
    {
        mt_async mt_unlocks_locks (mutex) GenericInformer::informAll_locked (
                proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data);
    }

    SubscriptionKey subscribe (T const        * const ev_struct,
                               void           * const cb_data,
                               VirtReferenced * const ref_data,
                               Object         * const guard_obj)
        { return subscribeVoid ((void*) ev_struct, cb_data, ref_data, guard_obj); }

    SubscriptionKey subscribe (CbDesc<T> const &cb)
        { return subscribeVoid ((void*) cb.cb, cb.cb_data, cb.ref_data, cb.guard_obj); }

    SubscriptionKey subscribe_locked (T const        * const ev_struct,
                                      void           * const cb_data,
                                      VirtReferenced * const ref_data,
                                      Object         * const guard_obj)
        { return subscribeVoid_locked ((void*) ev_struct, cb_data, ref_data, guard_obj); }

    SubscriptionKey subscribe_locked (CbDesc<T> const &cb)
        { return subscribeVoid_locked ((void*) cb.cb, cb.cb_data, cb.ref_data, cb.guard_obj); }

    Informer_ (Object     * const outer_object,
               StateMutex * const mutex)
        : GenericInformer (outer_object, mutex)
    {}
};

// Informer for direct callbacks.
template <class T>
class Informer : public GenericInformer
{
  public:
    typedef void (*InformCallback) (T     cb,
                                    void *cb_data,
                                    void *inform_data);

  protected:
    static void proxyInformCallback (CallbackPtr      const cb_ptr,
                                     void           * const cb_data,
                                     VoidFunction     const inform_cb,
                                     void           * const inform_data)
        { ((InformCallback) inform_cb) ((T) cb_ptr.func, cb_data, inform_data); }

  public:
    mt_async void informAll (InformCallback   const inform_cb,
                             void           * const inform_cb_data)
        { mt_async GenericInformer::informAll (proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data); }

    mt_async mt_unlocks_locks (mutex) void informAll_locked (InformCallback    const inform_cb,
                                                             void            * const inform_cb_data)
    {
        mt_async mt_unlocks_locks (mutex) GenericInformer::informAll_locked (
                proxyInformCallback, (VoidFunction) inform_cb, inform_cb_data);
    }

    SubscriptionKey subscribe (T                const cb,
                               void           * const cb_data,
                               VirtReferenced * const ref_data,
                               Object         * const guard_obj)
        { return subscribeVoid ((VoidFunction) cb, cb_data, ref_data, guard_obj); }

    SubscriptionKey subscribe (CbDesc<T> const &cb)
        { return subscribeVoid ((VoidFunction) cb.cb, cb.cb_data, cb.ref_data, cb.guard_obj); }

    mt_mutex (mutex) SubscriptionKey subscribe_locked (CbDesc<T> const &cb)
        { return subscribeVoid_locked ((VoidFunction) cb.cb, cb.cb_data, cb.ref_data, cb.guard_obj); }

    Informer (Object     * const mt_nonnull outer_object,
              StateMutex * const mutex)
        : GenericInformer (outer_object, mutex)
    {}
};

}


#endif /* LIBMARY__INFORMER__H__ */

