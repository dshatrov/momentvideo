/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/active_poll_group.h>
#include <libmary/log.h>

#include <libmary/deletion_context.h>


namespace M {

void
DeletionContext::scheduleDeletion (Object * const mt_nonnull obj)
{
    mutex.lock ();
    {
        Object * const last = queue_last;
        if (!last) {
            obj->atomic_shadow.set_nonatomic (obj);
        } else {
            obj->atomic_shadow.set_nonatomic (last->atomic_shadow.get_nonatomic());
            last->atomic_shadow.set_nonatomic (obj);
        }
        queue_last = obj;
    }
    mutex.unlock ();

    if (!active_poll_group->trigger ())
        logF_ (_func, "trigger() failed: ", exc->toString());
}

void
DeletionContext::process ()
{
    mutex.lock ();
    Object * const last = queue_last;
    queue_last = NULL;
    mutex.unlock ();

    {
        Object *obj = last;
        while (obj) {
            Object * const next_obj = static_cast <Object*> (obj->atomic_shadow.get_nonatomic());
            obj->deleteThis ();
            obj = next_obj;
        }
    }
}

}

