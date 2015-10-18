/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT_NVR__NVR_READER__H__
#define MOMENT_NVR__NVR_READER__H__


#include <moment-nvr/media_reader.h>


namespace MomentNvr {

mt_unsafe class NvrReader : public VodReader
{
  private:
    mt_const Ref<PagePool> page_pool;
    mt_const StRef<Vfs>    vfs;
    mt_const StRef<String> stream_name;

    Time   timelapse_frame_interval_millisec;
    Uint32 timelapse_frames_per_second;

    Ref<MediaReader> media_reader;
    Time seek_pos_microsec;

    Ref<MediaDesc_Audio> media_desc__audio;
    bool                 got_audio_desc;

    Ref<MediaDesc_Video> media_desc__video;
    bool                 got_video_desc;

    bool media_desc_reported;
    bool aac_cdata_reported;
    bool avc_cdata_reported;

    AudioMessage saved_audio_msg;
    bool         got_saved_audio_msg;

    VideoMessage saved_video_msg;
    bool         got_saved_video_msg;

    void reset ();

    mt_iface (MediaReader::ReadFrameBackend)
      static MediaReader::ReadFrameBackend const read_frame_backend;

      static MediaReader::ReadFrameResult mediaReader_audioFrame (AudioMessage * mt_nonnull audio_msg,
                                                                  void         *_self);

      static MediaReader::ReadFrameResult mediaReader_videoFrame (VideoMessage * mt_nonnull video_msg,
                                                                  void         *_self);
    mt_iface_end

    bool createMediaReaderIfNeeded (ReadResult * mt_nonnull ret_res);

    bool reportMediaDescIfNeeded (ReadMessage   * mt_nonnull read_msg,
                                  MediaMessage ** mt_nonnull ret_msg,
                                  bool           report_audio);

  public:
    mt_iface (VodReader)
      ReadResult readNextFrame (ReadMessage   * mt_nonnull read_msg,
                                MediaMessage ** mt_nonnull ret_msg,
                                bool           stop_time_enabled,
                                Time           stop_time_microsec);

      SeekResult seek (Time seek_pos_microsec);

      void setTimelapse (Time   const timelapse_frame_interval_millisec,
                         Uint32 const timelapse_frames_per_second)
      {
          this->timelapse_frame_interval_millisec = timelapse_frame_interval_millisec;
          this->timelapse_frames_per_second       = timelapse_frames_per_second;

          if (media_reader) {
              media_reader->setTimelapse (timelapse_frame_interval_millisec,
                                          timelapse_frames_per_second);
          }
      }

      Result getSourceInfo (VodSourceInfo * mt_nonnull source_info);
    mt_iface_end

    mt_const void init (PagePool    * mt_nonnull page_pool,
                        Vfs         * mt_nonnull vfs,
                        ConstMemory  stream_name);

     NvrReader ();
    ~NvrReader ();
};

}


#endif /* MOMENT_NVR__NVR_READER__H__ */

