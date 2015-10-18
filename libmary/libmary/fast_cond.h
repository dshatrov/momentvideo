/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__FAST_COND__H__
#define LIBMARY__FAST_COND__H__


#include <libmary/cond.h>


namespace M {

class FastCond
{
  private:
    Cond cond;

    Count num_waiters;
    Count num_signals;

  public:
    mt_mutex (mutex) void waitOrSpuriousWakeup (Mutex * const mt_nonnull mutex)
    {
        ++num_waiters;

        cond.wait (*mutex);

        if (num_signals) {
            --num_signals;
        } else {
          // there was a spurious wakeup
            assert (num_waiters);
            --num_waiters;
        }
    }

    mt_mutex (waitOrSpuriousWakeup::mutex) void signalAtLeastOne ()
    {
        if (num_waiters) {
            --num_waiters;
            ++num_signals;
            cond.signal ();
        }
    }

    mt_mutex (waitOrSpuriousWakeup::mutex) void broadcastAll ()
    {
        if (num_waiters) {
            num_signals = num_waiters;
            num_waiters = 0;
            cond.broadcastAll ();
        }
    }

    FastCond ()
        : num_waiters (0),
          num_signals (0)
    {}
};

}


#endif /* LIBMARY__FAST_COND__H__ */

