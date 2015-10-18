/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/informer.h>


namespace M {

// Informer is a helper object for signaling asynchronous events in MT-safe
// manner.

mt_mutex (mutex) void
GenericInformer::releaseSubscription (Subscription * const mt_nonnull sbn)
{
    if (sbn->del_sbn) {
        Ref<Object> const code_ref = sbn->weak_ref.getRef ();
        if (!code_ref) {
          // The subscription will be released by subscriberDeletionCallback,
          // which is likely just about to be called.
            return;
        }

        code_ref->removeDeletionCallback (sbn->del_sbn);
    }

    sbn_list.remove (sbn);
    // This is why we need StateMutex and not a plain Mutex:
    // 'sbn' carries a VirtRef to an arbitrary object.
    delete sbn;
}

void
GenericInformer::subscriberDeletionCallback (void * const _sbn)
{
    Subscription * const sbn = static_cast <Subscription*> (_sbn);
    GenericInformer * const self = sbn->informer;

    self->mutex->lock ();
    self->sbn_list.remove (sbn);
    self->mutex->unlock ();

    delete sbn;
}

mt_async void
GenericInformer::informAll (ProxyInformCallback   const mt_nonnull proxy_inform_cb,
			    VoidFunction          const inform_cb,
			    void                * const inform_cb_data)
{
    mutex->lock ();
    mt_async mt_unlocks_locks (mutex) informAll_locked (proxy_inform_cb, inform_cb, inform_cb_data);
    mutex->unlock ();
}

mt_async mt_unlocks_locks (mutex) void
GenericInformer::informAll_locked (ProxyInformCallback   const mt_nonnull proxy_inform_cb,
                                   VoidFunction          const inform_cb,
                                   void                * const inform_cb_data)
{
    ++traversing;

    Subscription *sbn = sbn_list.getFirst();
    while (sbn) {
	if (!sbn->valid) {
	    sbn = sbn_list.getNext (sbn);
	    continue;
	}

	if (sbn->oneshot)
	    sbn->valid = false;

	Ref<Object> code_ref;
	if (sbn->weak_ref.isValid()) {
	    LibMary_ThreadLocal * const tlocal = libMary_getThreadLocal();
	    if (sbn->weak_ref.getShadowPtr() != tlocal->last_coderef_container_shadow) {
		code_ref = sbn->weak_ref.getRef ();
		if (code_ref) {
		    mutex->unlock ();

                    Object::Shadow * const prv_coderef_container_shadow = tlocal->last_coderef_container_shadow;
		    tlocal->last_coderef_container_shadow = sbn->weak_ref.getShadowPtr();

		    proxy_inform_cb (sbn->cb_ptr, sbn->cb_data, inform_cb, inform_cb_data);

		    tlocal->last_coderef_container_shadow = prv_coderef_container_shadow;

		    mutex->lock ();
		    sbn = sbn_list.getNext (sbn);
		    continue;
		} else {
		    sbn = sbn_list.getNext (sbn);
		    continue;
		}
            }
        }

	mutex->unlock ();

	proxy_inform_cb (sbn->cb_ptr, sbn->cb_data, inform_cb, inform_cb_data);

        // Nullifying with 'mutex' unlocked.
        code_ref = NULL;

	mutex->lock ();
	sbn = sbn_list.getNext (sbn);
    }

    --traversing;
    if (traversing == 0) {
	sbn = sbn_invalidation_list.getFirst();
	while (sbn) {
	    Subscription * const next_sbn = sbn_invalidation_list.getNext (sbn);
	    assert (!sbn->valid);

	    releaseSubscription (sbn);

	    sbn = next_sbn;
	}
	sbn_invalidation_list.clear ();
    }
}

GenericInformer::SubscriptionKey
GenericInformer::subscribeVoid (CallbackPtr      const cb_ptr,
				void           * const cb_data,
				VirtReferenced * const ref_data,
				Object         * const guard_obj) 
{
    Subscription * const sbn =
            new (std::nothrow) Subscription (cb_ptr, cb_data, ref_data, guard_obj);
    sbn->valid = true;
    sbn->informer = this;
    sbn->oneshot = false;

    if (guard_obj) {
	sbn->del_sbn = guard_obj->addDeletionCallback (
                CbDesc<Object::DeletionCallback> (
                        subscriberDeletionCallback,
                        sbn,
                        outer_object));
    }

    mutex->lock ();
    sbn_list.prepend (sbn);
    mutex->unlock ();

    return sbn;
}

mt_mutex (mutex) GenericInformer::SubscriptionKey
GenericInformer::subscribeVoid_locked (CallbackPtr      const cb_ptr,
                                       void           * const cb_data,
                                       VirtReferenced * const ref_data,
                                       Object         * const guard_obj)
{
    Subscription * const sbn =
            new (std::nothrow) Subscription (cb_ptr, cb_data, ref_data, guard_obj);
    sbn->valid = true;
    sbn->informer = this;
    sbn->oneshot = false;

    if (guard_obj) {
	sbn->del_sbn = guard_obj->addDeletionCallback (
                CbDesc<Object::DeletionCallback> (
                        subscriberDeletionCallback,
                        sbn,
                        outer_object));
    }

    sbn_list.prepend (sbn);

    return sbn;
}

void
GenericInformer::unsubscribe (SubscriptionKey const sbn_key)
{
    mutex->lock ();
    unsubscribe_locked (sbn_key);
    mutex->unlock ();
}

mt_mutex (mutex) void
GenericInformer::unsubscribe_locked (SubscriptionKey const sbn_key)
{
    sbn_key.sbn->valid = false;
    if (traversing == 0) {
	releaseSubscription (sbn_key.sbn);
        return;
    }
    sbn_invalidation_list.append (sbn_key.sbn);
}

GenericInformer::~GenericInformer ()
{
    mutex->lock ();

    assert (sbn_invalidation_list.isEmpty());

    {
        Subscription *sbn = sbn_list.getFirst();
        while (sbn) {
            Subscription * const next_sbn = sbn_list.getNext (sbn);
            delete sbn;
            sbn = next_sbn;
        }
    }

    mutex->unlock ();
}

}

