/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/mp4_av_muxer.h>


namespace Moment {
    
//Size const mdat_size = 8 + audio_track.total_frame_size + video_track.total_frame_size;

static Byte const mp4_header [] = {
    // ftyp
    0x00, 0x00, 0x00, 0x20,
     'f',  't',  'y',  'p',
     'm',  'p',  '4',  '2',
    0x00, 0x00, 0x00, 0x00,
     'm',  'p',  '4',  '2',
     'm',  'p',  '4',  '1',
     'i',  's',  'o',  'm',
     'i',  's',  'o',  '2',
  /* For QT:
     'q',  't',  ' ',  ' ',
    0x20, 0x04, 0x06, 0x00,
     'q',  't',  ' ',  ' '
   */

    // mdat
    0,
    0,
    0,
    0,
    'm',  'd',  'a',  't'
};

mt_throws Result
Mp4AvMuxer::beginMuxing ()
{
    SenderMessageEntry_Pages * const msg_pages =
            SenderMessageEntry_Pages::createNew (sizeof (mp4_header));

    msg_pages->init (/*first_page=*/ NULL, page_pool, /*msg_offs=*/ 0, /*msg_len=*/ 0);

    memcpy (msg_pages->getHeaderData(), mp4_header, sizeof (mp4_header));
    msg_pages->header_len = sizeof (mp4_header);

    sender->sendMessage (msg_pages, true /* do_flush */);

    return Result::Success;
}

void
Mp4AvMuxer::doMuxMessage (MediaMessage * const mt_nonnull msg)
{
    if (!got_first_dts) {
        got_first_dts = true;
        first_dts_nanosec = msg->dts_nanosec;
    }
    last_dts_nanosec = msg->dts_nanosec;

    if (msg->msg_len >= (1 << 24)) {
        logD_ (_this_func, "Message is too long (", msg->msg_len, " bytes), dropping it");
        return;
    }

    total_frame_size += msg->msg_len;

    {
        SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
        msg_pages->init (msg->page_list.first, msg->page_pool, msg->msg_offset, msg->msg_len);
        msg->page_pool->msgRef (msg->page_list.first);

        sender->sendMessage (msg_pages, true /* do_flush */);
    }
}

mt_throws Result
Mp4AvMuxer::muxAudioMessage (AudioMessage * const mt_nonnull msg)
{
    if (msg->frame_type == AudioFrameType::AacSequenceHeader) {
        mp4_muxer.pass1_aacSequenceHeader (msg->page_pool,
                                           msg->page_list.first,
                                           msg->msg_offset,
                                           msg->msg_len);
    } else
    if (msg->frame_type.isAudioData()) {
        mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Audio,
                               msg->pts_nanosec,
                               msg->dts_nanosec,
                               msg->msg_len,
                               false /* is_sync_sample */);
        doMuxMessage (msg);
    }

    return Result::Success;
}

mt_throws Result
Mp4AvMuxer::muxVideoMessage (VideoMessage * const mt_nonnull msg)
{
    if (msg->frame_type == VideoFrameType::AvcSequenceHeader) {
        mp4_muxer.pass1_avcSequenceHeader (msg->page_pool,
                                           msg->page_list.first,
                                           msg->msg_offset,
                                           msg->msg_len);
    } else
    if (msg->frame_type.isVideoData()) {
        mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Video,
                               msg->pts_nanosec,
                               msg->dts_nanosec,
                               msg->msg_len,
                               msg->frame_type.isKeyFrame());
        doMuxMessage (msg);
    }

    return Result::Success;
}

mt_throws Result
Mp4AvMuxer::endMuxing ()
{
    logD_ (_this_func_);

    {
        Uint64 duration_nanosec = 0;
        if (last_dts_nanosec > first_dts_nanosec)
            duration_nanosec = last_dts_nanosec - first_dts_nanosec;

        PagePool::PageListInfo const moov_header =
                mp4_muxer.pass1_complete (duration_nanosec / 1000000, true /* moov_only */, true /* moov_tail */);

        {
            SenderMessageEntry_Pages * const msg_pages = SenderMessageEntry_Pages::createNew (/*header_len=*/ 0);
            msg_pages->init (moov_header.first, page_pool, /*msg_offs=*/ 0, moov_header.data_len);

            sender->sendMessage (msg_pages, true /* do_flush */);
        }
    }

    sender->seekSet (sizeof (mp4_header) - 8);
    {
        Size const mdat_size = 8 + total_frame_size;

        Byte const buf [4] = {
            (Byte) ((mdat_size >> 24) & 0xff),
            (Byte) ((mdat_size >> 16) & 0xff),
            (Byte) ((mdat_size >>  8) & 0xff),
            (Byte) ((mdat_size >>  0) & 0xff)
        };

        SenderMessageEntry_Pages * const msg_pages =
                SenderMessageEntry_Pages::createNew (sizeof (buf));
        msg_pages->init (/*first_page=*/ NULL, /*page_pool=*/ NULL, /*msg_offs=*/ 0, /*msg_len=*/ 0);

        memcpy (msg_pages->getHeaderData(), buf, sizeof (buf));
        msg_pages->header_len = sizeof (buf);

        sender->sendMessage (msg_pages, true /* do_flush */);
    }

    sender->closeAfterFlush ();
    return Result::Success;
}

Mp4AvMuxer::Mp4AvMuxer ()
    : page_pool         (NULL),
      total_frame_size  (0),
      got_first_dts     (false),
      first_dts_nanosec (0),
      last_dts_nanosec  (0)
{
}

}

