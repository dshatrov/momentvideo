/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__DELETION_QUEUE__H__
#define LIBMARY__DELETION_QUEUE__H__


#include <libmary/object.h>


namespace M {

void deletionQueue_append (Object *obj);

void deletionQueue_process ();

// This is only used for a debug warning when going to wait for a condition
// with a state mutex held. Perhaps something could be done about this.
bool deletionQueue_isEmpty ();

void deletionQueue_init ();

}


#endif /* LIBMARY__DELETION_QUEUE__H__ */

