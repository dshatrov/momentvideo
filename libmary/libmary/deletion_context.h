/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__DELETION_CONTEXT__H__
#define LIBMARY__DELETION_CONTEXT__H__


#include <libmary/mutex.h>


namespace M {

class Object;
class ActivePollGroup;

class DeletionContext
{
  private:
    RawMutex mutex;

    mt_const ActivePollGroup *active_poll_group;

    mt_mutex (mutex) Object *queue_last;

  public:
    void scheduleDeletion (Object * mt_nonnull obj);

    void process ();

    mt_const void init (ActivePollGroup * const mt_nonnull active_poll_group)
        { this->active_poll_group = active_poll_group; }

    DeletionContext ()
        : active_poll_group (NULL),
          queue_last (NULL)
    {}
};

}


#include <libmary/object.h>


#endif /* LIBMARY__DELETION_CONTEXT__H__ */

