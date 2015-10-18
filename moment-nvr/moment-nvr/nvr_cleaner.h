/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__NVR_CLEANER__H__
#define MOMENT_NVR__NVR_CLEANER__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace Moment;

class NvrCleaner : public Object
{
  private:
    mt_const StRef<Vfs> vfs;
    mt_const StRef<String> stream_name;
    mt_const Time max_age_sec;

    void doRemoveFiles (ConstMemory vdat_filename,
                        ConstMemory idx_filename);

    static void cleanupTimerTick (void *_self);

  public:
    mt_const void init (Timers      * mt_nonnull timers,
                        Vfs         * mt_nonnull vfs,
                        ConstMemory  stream_name,
                        Time         max_age_sec,
                        Time         clean_interval_sec);

    NvrCleaner (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT_NVR__NVR_CLEANER__H__ */

