/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__RTMP_SERVER__H__
#define LIBMOMENT__RTMP_SERVER__H__


#include <moment/stream.h>


namespace Moment {

using namespace M;

class RtmpServer
{
public:
    class MetaData
    {
    public:
      // GStreamer's FLV muxer:

	// General:
	//   duration
	//   filesize
	//   creator
	//   title

	// Video:
	//   videocodecid
	//   width
	//   height
	//   AspectRatioX
	//   AspectRatioY
	//   framerate

	// Audio:
	//   audiocodecid

	// creationdate

      // Ffmpeg FLV muxer:

	// General:
	//   duration
	//   filesize

	// Video:
	//   width
	//   height
	//   videodatarate
	//   framerate
	//   videocodecid

	// Audio:
	//   audiodatarate
	//   audiosamplerate
	//   audiosamplesize
	//   stereo
	//   audiocodecid

	enum GotFlags {
	    Got_VideoCodecId = 0x0001,
	    Got_AudioCodecId = 0x0002,

	    VideoWidth      = 0x0004,
	    VideoHeight     = 0x0008,

	    AspectRatioX    = 0x0010,
	    AspectRatioY    = 0x0020,

	    VideoDataRate   = 0x0040,
	    AudioDataRate   = 0x0080,

	    VideoFrameRate  = 0x0100,

	    AudioSampleRate = 0x0200,
	    AudioSampleSize = 0x0400,

	    NumChannels     = 0x0800
	};

	VideoCodecId video_codec_id;
	AudioCodecId audio_codec_id;

	Uint32 video_width;
	Uint32 video_height;

	Uint32 aspect_ratio_x;
	Uint32 aspect_ratio_y;

	Uint32 video_data_rate;
	Uint32 audio_data_rate;

	Uint32 video_frame_rate;

	Uint32 audio_sample_rate;
	Uint32 audio_sample_size;

	Uint32 num_channels;

	Uint32 got_flags;

	MetaData ()
	    : got_flags (0)
	{}
    };

    static Result encodeMetaData (MetaData     * mt_nonnull metadata,
				  PagePool     * mt_nonnull page_pool,
				  VideoMessage *ret_msg);
};

}


#endif /* LIBMOMENT__RTMP_SERVER__H__ */

