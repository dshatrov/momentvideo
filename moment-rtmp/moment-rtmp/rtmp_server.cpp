/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/amf_encoder.h>
#include <moment/inc.h>

#include <moment-rtmp/rtmp_server.h>


namespace Moment {

Result
RtmpServer::encodeMetaData (MetaData     * const mt_nonnull metadata,
			    PagePool     * const mt_nonnull page_pool,
			    VideoMessage * const ret_msg)
{
    PagePool::PageListHead page_list;

    AmfAtom atoms [128];
    AmfEncoder encoder (atoms);

    encoder.addString ("onMetaData");

    encoder.beginEcmaArray (0 /* num_entries */);
    AmfAtom * const toplevel_array_atom = encoder.getLastAtom ();
    Uint32 num_entries = 0;

    if (metadata->got_flags & MetaData::Got_VideoCodecId) {
	encoder.addFieldName ("videocodecid");
	encoder.addNumber (metadata->video_codec_id);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::Got_AudioCodecId) {
	encoder.addFieldName ("audiocodecid");
	encoder.addNumber (metadata->audio_codec_id);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoWidth) {
	encoder.addFieldName ("width");
	encoder.addNumber (metadata->video_width);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoHeight) {
	encoder.addFieldName ("height");
	encoder.addNumber (metadata->video_height);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AspectRatioX) {
	encoder.addFieldName ("AspectRatioX");
	encoder.addNumber (metadata->aspect_ratio_x);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AspectRatioY) {
	encoder.addFieldName ("AspectRatioY");
	encoder.addNumber (metadata->aspect_ratio_y);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoDataRate) {
	encoder.addFieldName ("videodatarate");
	encoder.addNumber (metadata->video_data_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioDataRate) {
	encoder.addFieldName ("audiodatarate");
	encoder.addNumber (metadata->audio_data_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::VideoFrameRate) {
	encoder.addFieldName ("framerate");
	encoder.addNumber (metadata->video_frame_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioSampleRate) {
	encoder.addFieldName ("audiosamplerate");
	encoder.addNumber (metadata->audio_sample_rate);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::AudioSampleSize) {
	encoder.addFieldName ("audiosamplesize");
	encoder.addNumber (metadata->audio_sample_size);
	++num_entries;
    }

    if (metadata->got_flags & MetaData::NumChannels) {
	encoder.addFieldName ("stereo");
	if (metadata->num_channels >= 2) {
	    encoder.addBoolean (true);
	} else {
	    encoder.addBoolean (false);
	}
	++num_entries;
    }

    encoder.endObject ();

    toplevel_array_atom->setEcmaArraySize (num_entries);

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
	logE_ (_func, "encode() failed");
	return Result::Failure;
    }

    page_pool->getFillPages (&page_list, ConstMemory (msg_buf, msg_len));

    if (ret_msg) {
	ret_msg->pts_nanosec = 0;
        ret_msg->dts_nanosec = 0;
	ret_msg->frame_type = VideoFrameType::RtmpSetMetaData;
	ret_msg->codec_id = VideoCodecId::Unknown;

	ret_msg->page_pool = page_pool;
	ret_msg->page_list = page_list;
	ret_msg->msg_len = msg_len;
	ret_msg->msg_offset = 0;
    }

    return Result::Success;
}

}

