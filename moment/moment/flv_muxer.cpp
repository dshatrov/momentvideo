/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/flv_util.h>

#include <moment/flv_muxer.h>


namespace Moment {

static LogGroup libMary_logGroup_flvmux ("flvmux", LogLevel::I);

static Byte const flv_header [] = {
    0x46, // 'F'
    0x4c, // 'L'
    0x56, // 'V'

    0x01, // FLV version 1
    0x05, // Audio and video tags are present

    // Data offset
    0x0,
    0x0,
    0x0,
    0x9,

    // Previous tag size
    0x0,
    0x0,
    0x0,
    0x0
};

mt_throws Result
FlvMuxer::beginMuxing ()
{
    {
        SenderMessageEntry_Pages * const msg_pages =
                SenderMessageEntry_Pages::createNew (sizeof (flv_header));
        msg_pages->init (/*first_page=*/ NULL, page_pool, /*msg_offs=*/ 0, /*msg_len=*/ 0);

        memcpy (msg_pages->getHeaderData(), flv_header, sizeof (flv_header));
        msg_pages->header_len = sizeof (flv_header);

        sender->sendMessage (msg_pages, true /* do_flush */);
    }

    muxMetadata ();

    return Result::Success;
}

void
FlvMuxer::muxMetadata ()
{
    PagePool::PageListHead page_list;

    AmfAtom atoms [128];
    AmfEncoder encoder (atoms);

    encoder.addString ("onMetaData");

    encoder.beginEcmaArray (0 /* num_entries */);
    AmfAtom * const toplevel_array_atom = encoder.getLastAtom ();
    Uint32 num_entries = 0;

    /*
    {
        encoder.addFieldName ("hasAudio");
        encoder.addBoolean (true);
        ++num_entries;
    }

    {
        encoder.addFieldName ("hasVideo");
        encoder.addBoolean (true);
        ++num_entries;
    }
    */

    {
        Uint64 duration_millisec = 0;
        if (got_first_timestamp)
            duration_millisec = (last_timestamp_nanosec - first_timestamp_nanosec) / 1000000;

        encoder.addFieldName ("duration");
        encoder.addNumber ((double) duration_millisec / 1000.0);
        ++num_entries;
    }

    /*
    {
        encoder.addFieldName ("hasCuePoints");
        encoder.addBoolean (false);
        ++num_entries;
    }
    */

    /*
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
    */

    encoder.endObject ();

    toplevel_array_atom->setEcmaArraySize (num_entries);

    Byte msg_buf [4096];
    Size msg_len;
    if (!encoder.encode (Memory::forObject (msg_buf), AmfEncoding::AMF0, &msg_len)) {
        logE_ (_func, "encode() failed");
        return;
    }

    page_pool->getFillPages (&page_list, ConstMemory (msg_buf, msg_len));

    Byte const tag_header [] = {
        0x12 /* unencrypted */,

        // Data size
        (Byte) ((msg_len >> 16) & 0xff),
        (Byte) ((msg_len >>  8) & 0xff),
        (Byte) ((msg_len >>  0) & 0xff),

        // Timestamp
        0, 0, 0,
        // Extended timestamp
        0,

        // Stream ID
        0,
        0,
        0
    };

    {
        SenderMessageEntry_Pages * const msg_pages =
                SenderMessageEntry_Pages::createNew (sizeof (tag_header));
        msg_pages->init (page_list.first, page_pool, /*msg_offs=*/ 0, msg_len);

        memcpy (msg_pages->getHeaderData(), tag_header, sizeof (tag_header));
        msg_pages->header_len = sizeof (tag_header);

        page_pool->msgRef (page_list.first);

        sender->sendMessage (msg_pages, false /* do_flush */);
    }

    {
        Size const tag_size = msg_len + sizeof (tag_header); 

        Byte const tag_footer [] = {
            (Byte) ((tag_size >> 24) & 0xff),
            (Byte) ((tag_size >> 16) & 0xff),
            (Byte) ((tag_size >>  8) & 0xff),
            (Byte) ((tag_size >>  0) & 0xff)
        };

        SenderMessageEntry_Pages * const msg_pages =
                SenderMessageEntry_Pages::createNew (sizeof (tag_footer));
        msg_pages->init (/*first_page=*/ NULL, /*page_pool=*/ NULL, /*msg_offs=*/ 0, /*msg_len=*/ 0);

        memcpy (msg_pages->getHeaderData(), tag_footer, sizeof (tag_footer));
        msg_pages->header_len = sizeof (tag_footer);

        sender->sendMessage (msg_pages, true /* do_flush */);
    }
}

void
FlvMuxer::doMuxMessage (MediaMessage * const mt_nonnull msg,
			Byte           const msg_type,
                        ConstMemory    const msg_header)
{
    if (!got_first_timestamp) {
        got_first_timestamp = true;
        first_timestamp_nanosec = msg->dts_nanosec;
    }
    last_timestamp_nanosec = msg->dts_nanosec;

    Uint64 const timestamp_millisec = msg->pts_nanosec / 1000000;

    if (msg->msg_len >= (1 << 24)) {
	logE (flvmux, _func, "Message is too long (", msg->msg_len, " bytes), dropping it");
	return;
    }

    Byte const tag_header [] = {
	msg_type /* unencrypted */,

	// Data size
	(Byte) (((msg->msg_len + msg_header.len()) >> 16) & 0xff),
	(Byte) (((msg->msg_len + msg_header.len()) >>  8) & 0xff),
	(Byte) (((msg->msg_len + msg_header.len()) >>  0) & 0xff),

	// Timestamp
	(Byte) ((timestamp_millisec >> 16) & 0xff),
	(Byte) ((timestamp_millisec >>  8) & 0xff),
	(Byte) ((timestamp_millisec >>  0) & 0xff),

	// Extended timestamp
	(Byte) ((timestamp_millisec >> 24) & 0xff),

	// Stream ID
	0,
	0,
	0
    };

    {
	SenderMessageEntry_Pages * const msg_pages =
		SenderMessageEntry_Pages::createNew (sizeof (tag_header) + msg_header.len());
        msg_pages->init (msg->page_list.first, msg->page_pool, msg->msg_offset, msg->msg_len);
        msg->page_pool->msgRef (msg->page_list.first);

	memcpy (msg_pages->getHeaderData(), tag_header, sizeof (tag_header));
        memcpy (msg_pages->getHeaderData() + sizeof (tag_header), msg_header.mem(), msg_header.len());
	msg_pages->header_len = sizeof (tag_header) + msg_header.len();

	sender->sendMessage (msg_pages, false /* do_flush */);
    }

    {
	Size const tag_size = msg->msg_len + sizeof (tag_header) + msg_header.len(); 

	Byte const tag_footer [] = {
	    (Byte) ((tag_size >> 24) & 0xff),
	    (Byte) ((tag_size >> 16) & 0xff),
	    (Byte) ((tag_size >>  8) & 0xff),
	    (Byte) ((tag_size >>  0) & 0xff)
	};

	SenderMessageEntry_Pages * const msg_pages =
		SenderMessageEntry_Pages::createNew (sizeof (tag_footer));
        msg_pages->init (/*first_page=*/ NULL, /*page_pool=*/ NULL, /*msg_offset=*/ 0, /*msg_len=*/ 0);

	memcpy (msg_pages->getHeaderData(), tag_footer, sizeof (tag_footer));
	msg_pages->header_len = sizeof (tag_footer);

	sender->sendMessage (msg_pages, true /* do_flush */);
    }
}

mt_throws Result
FlvMuxer::muxAudioMessage (AudioMessage * const mt_nonnull msg)
{
    Byte flv_audio_header [FlvAudioHeader_MaxLen];
    unsigned const flv_audio_header_len = fillFlvAudioHeader (msg, Memory::forObject (flv_audio_header));
    if (flv_audio_header_len == 0 && msg->msg_len != 0) {
        logD_ (_func, "Ignoring non-empty audio message: couldn't fill audio header");
        return Result::Success;
    }

    logD (flvmux, _func, "pts ", msg->pts_nanosec / 1000000);
    doMuxMessage (msg, 0x8 /* audio tag */, ConstMemory (flv_audio_header, flv_audio_header_len));
    return Result::Success;
}

mt_throws Result
FlvMuxer::muxVideoMessage (VideoMessage * const mt_nonnull msg)
{
    Byte flv_video_header [FlvVideoHeader_MaxLen];
    unsigned const flv_video_header_len = fillFlvVideoHeader (msg, Memory::forObject (flv_video_header));
    if (flv_video_header_len == 0 && msg->msg_len != 0) {
        logD_ (_func, "Ignoring non-empty video message: couldn't fill video header");
        return Result::Success;
    }

    logD (flvmux, _func, "pts ", msg->pts_nanosec / 1000000);
    doMuxMessage (msg, 0x9 /* video tag */, ConstMemory (flv_video_header, flv_video_header_len));
    return Result::Success;
}

mt_throws Result
FlvMuxer::endMuxing ()
{
    logD_ (_this_func_);
    sender->seekSet (sizeof (flv_header));
    muxMetadata ();
    sender->closeAfterFlush ();
    return Result::Success;
}

FlvMuxer::FlvMuxer ()
    : page_pool (NULL),
      got_first_timestamp     (false),
      first_timestamp_nanosec (0),
      last_timestamp_nanosec  (0)
{
}

}

