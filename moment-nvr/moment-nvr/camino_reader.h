/*  Copyright (C) 2015 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__CAMINO_READER__H__
#define MOMENT_NVR__CAMINO_READER__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace Moment;

mt_unsafe class CaminoReader : public VodReader
{
  private:
    mt_const Ref<PagePool> page_pool;
    mt_const StRef<Vfs> vfs;

  public:
    mt_iface (VodReader)
      ReadResult readNextFrame (ReadMessage   * mt_nonnull read_msg,
                                MediaMessage ** mt_nonnull ret_msg,
                                bool           stop_time_enabled,
                                Time           stop_time_microsec);

      SeekResult seek (Time seek_pos_microsec);

      void setTimelapse (Time   timelapse_frame_interval_millisec,
                         Uint32 timelapse_frames_per_second);

      Result getSourceInfo (VodSourceInfo * mt_nonnull source_info);
    mt_iface_end

    mt_const void init (PagePool    * mt_nonnull page_pool,
                        Vfs         * mt_nonnull vfs,
                        ConstMemory  stream_name);
};

}


#endif /* MOMENT_NVR__CAMINO_READER__H__ */

