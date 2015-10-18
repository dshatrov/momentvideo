/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__NAMING_SCHEME__H__
#define MOMENT_NVR__NAMING_SCHEME__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace Moment;

struct NamingScheme : public Referenced
{
    virtual StRef<String> getPath (ConstMemory  channel_name,
                                   Time         unixtime_sec,
                                   Time        *ret_next_unixtime_sec) = 0;
};

class DefaultNamingScheme : public NamingScheme
{
  private:
    Time  const file_duration_sec;
    Int64 const offset_hours;

  public:
    StRef<String> getPath (ConstMemory  channel_name,
                           Time         unixtime_sec,
                           Time        *ret_next_unixtime_sec);

    DefaultNamingScheme (Time  const file_duration_sec,
                         Int64 const offset_hours)
        : file_duration_sec (file_duration_sec),
          offset_hours (offset_hours)
    {}
};

}


#endif /* MOMENT_NVR__NAMING_SCHEME__H__ */

