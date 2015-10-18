/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/mtp_connection.h>


#define MOMENT__MTP_CONNECTION__HEADER_SIZE 26


namespace Moment {

mt_sync_domain (receiver) void
MtpConnection::processInMessage ()
{
    logD_ (_this_func_);

    VideoStream * const stream = stream_group->getStream (in_msg_stream_id - 1);
    if (!stream) {
        logD_ (_func, "unexpected stream_id ", in_msg_stream_id);
        return;
    }

    switch (in_msg_type) {
        case MessageType::Audio: {
            StreamEntry *stream_entry = NULL;
            {
                StreamMap::Entry const entry = stream_map.lookup (in_msg_stream_id);
                if (entry.isNull()) {
                    logD_ (_func, "no media desc: stream_id ", in_msg_stream_id);
                    return;
                }

                stream_entry = &entry.getData();
                if (!stream_entry->media_desc__audio)
                    logD_ (_func, "no audio media desc: stream_id ", in_msg_stream_id);
            }

            AudioMessage audio_msg;

            audio_msg.pts_nanosec = in_msg_pts_nanosec;
            audio_msg.dts_nanosec = in_msg_dts_nanosec;

            audio_msg.page_pool = page_pool;
            audio_msg.page_list = in_page_list;

            audio_msg.msg_len = in_msg_len;
            audio_msg.msg_offset = 0;

            audio_msg.frame_type = AudioFrameType::RawData;
            audio_msg.codec_id = stream_entry->media_desc__audio->audio_codec_id;
            audio_msg.rate     = stream_entry->media_desc__audio->rate;
            audio_msg.channels = stream_entry->media_desc__audio->channels;

            stream->fireAudioMessage (&audio_msg);
        } break;
        case MessageType::Video: {
            StreamEntry *stream_entry = NULL;
            {
                StreamMap::Entry const entry = stream_map.lookup (in_msg_stream_id);
                if (entry.isNull()) {
                    logD_ (_func, "no media desc: stream_id ", in_msg_stream_id);
                    return;
                }

                stream_entry = &entry.getData();
                if (!stream_entry->media_desc__video)
                    logD_ (_func, "no video media desc: stream_id ", in_msg_stream_id);
            }

            VideoMessage video_msg;

            video_msg.pts_nanosec = in_msg_pts_nanosec;
            video_msg.dts_nanosec = in_msg_dts_nanosec;

            video_msg.page_pool = page_pool;
            video_msg.page_list = in_page_list;

            video_msg.msg_len = in_msg_len;
            video_msg.msg_offset = 0;

            {
                if (in_msg_frame_type >= VideoFrameType::Last) {
                    logD_ (_func, "invalid video frame type: ", in_msg_frame_type);
                    return;
                }

                video_msg.frame_type = (VideoFrameType::Value) in_msg_frame_type;
            }

            video_msg.codec_id = stream_entry->media_desc__video->video_codec_id;

            stream->fireVideoMessage (&video_msg);
        } break;
        case MessageType::MediaDesc: {
            logD_ (_this_func, "MediaDesc, in_msg_stream_id ", in_msg_stream_id);

            Ref<MediaDesc> const media_desc = decodeMediaDesc (in_page_list.first, 0 /* offs */, in_msg_len);
            if (!media_desc) {
                logD_ (_this_func, "decodeMediaDesc() failed");
                return;
            }

            StreamEntry *stream_entry = NULL;
            {
                StreamMap::Entry const entry = stream_map.lookup (in_msg_stream_id);
                if (!entry.isNull()) {
                    stream_entry = &entry.getData();
                } else {
                    if (in_num_streams >= 128) {
                        logD_ (_this_func, "too many streams");
                        return;
                    }

                    logD_ (_this_func, "MediaDesc stream_map.addFor (", in_msg_stream_id, ")");
                    stream_entry = &stream_map.addFor (in_msg_stream_id).getData();
                    stream_entry->stream_id = in_msg_stream_id;
                    ++in_num_streams;
                }
            }

            if (media_desc->getType() == MediaDesc::Audio) {
                stream_entry->media_desc__audio = static_cast <MediaDesc_Audio*> (media_desc.ptr());
            } else {
                assert (media_desc->getType() == MediaDesc::Video);
                stream_entry->media_desc__video = static_cast <MediaDesc_Video*> (media_desc.ptr());
            }
        } break;
        case MessageType::BeginMediaDesc: {
            in_media_desc_began = true;
        } break;
        case MessageType::EndMediaDesc: {
            in_media_desc_began = false;

            StreamEntry *stream_entry = NULL;
            {
                logD_ (_this_func, "EndMediaDesc in_msg_stream_id ", in_msg_stream_id);
                StreamMap::Entry const entry = stream_map.lookup (in_msg_stream_id);
                if (entry.isNull()) {
                    logD_ (_func, "unexpected stream_id ", in_msg_stream_id);
                    return;
                }

                stream_entry = &entry.getData();
            }

            stream->fireMediaDesc (stream_entry->media_desc__audio.ptr(),
                                   true /* set_audio */,
                                   stream_entry->media_desc__video.ptr(),
                                   true /* set_video */);
        } break;
        case MessageType::Unknown: {
            logD_ (_this_func, "Unknown message type");
        } break;
    }
}

Receiver::Frontend const MtpConnection::receiver_frontend = {
    processInput,
    processEof,
    processError,
    NULL /* processInputFrom */
};

mt_sync_domain (receiver) Receiver::ProcessInputResult
MtpConnection::processInput (Memory   const mem,
                             Size   * const mt_nonnull ret_accepted,
                             void   * const _self)
{
    MtpConnection * const self = static_cast <MtpConnection*> (_self);
    logD_ (_self_func, "len ", mem.len());

    Byte *buf = mem.mem();
    Size  len = mem.len();

    *ret_accepted = 0;

    for (;;) {
        switch (self->in_recv_state) {
            case ReceiveState::MessageHeader: {
                if (len < MOMENT__MTP_CONNECTION__HEADER_SIZE) {
                    logD_ (_self_func, "Again, ret_accepted ", *ret_accepted);
                    return Receiver::ProcessInputResult::Again;
                }

                Size pos = 0;

                self->in_msg_stream_id = readBe32 (buf + pos);
                pos += 4;

                {
                    Byte const mtype = buf [pos];
                    switch (mtype) {
                        case 1:  self->in_msg_type = MessageType::Audio;          break;
                        case 2:  self->in_msg_type = MessageType::Video;          break;
                        case 3:  self->in_msg_type = MessageType::MediaDesc;      break;
                        case 4:  self->in_msg_type = MessageType::BeginMediaDesc; break;
                        case 5:  self->in_msg_type = MessageType::EndMediaDesc;   break;
                        default: self->in_msg_type = MessageType::Unknown;        break;
                    }
                }
                ++pos;

                self->in_msg_pts_nanosec = readBe64 (buf + pos);
                pos += 8;

                self->in_msg_dts_nanosec = readBe64 (buf + pos);
                pos += 8;

                self->in_msg_len = readBe32 (buf + pos);
                pos += 4;

                self->in_msg_frame_type = buf [pos];
                pos += 1;

                self->in_msg_left = self->in_msg_len;
                self->in_recv_state = ReceiveState::MessageBody;

                buf += MOMENT__MTP_CONNECTION__HEADER_SIZE;
                len -= MOMENT__MTP_CONNECTION__HEADER_SIZE;
                *ret_accepted += MOMENT__MTP_CONNECTION__HEADER_SIZE;
            } break;
            case ReceiveState::MessageBody: {
                if (self->in_msg_left > 0) {
                    if (len == 0) {
                        logD_ (_self_func, "Normal, ret_accepted ", *ret_accepted);
                        return Receiver::ProcessInputResult::Normal;
                    }

                    Size tofill = self->in_msg_left;
                    if (tofill > len)
                        tofill = len;

                    self->page_pool->getFillPages (&self->in_page_list,
                                                   ConstMemory (buf, tofill));

                    self->in_msg_left -= tofill;
                    buf += tofill;
                    len -= tofill;
                    *ret_accepted += tofill;
                } else {
                    self->processInMessage ();

                    self->page_pool->msgUnref (self->in_page_list.first);
                    self->in_page_list.reset ();

                    self->in_recv_state = ReceiveState::MessageHeader;
                }
            } break;
        }
    }

    logD_ (_self_func, "Normal, ret_accepted ", *ret_accepted);
    return Receiver::ProcessInputResult::Normal;
}

mt_sync_domain (receiver) void
MtpConnection::processEof (Memory   const /* unprocessed_mem */,
                           void   * const _self)
{
    MtpConnection * const self = static_cast <MtpConnection*> (_self);

    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

mt_sync_domain (receiver) void
MtpConnection::processError (Exception * const /* exc_ */,
                             Memory      const /* unprocessed_mem */,
                             void      * const _self)
{
    MtpConnection * const self = static_cast <MtpConnection*> (_self);

    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

Sender::Frontend const MtpConnection::sender_frontend = {
    NULL /* sendStateChanged */,
    senderClosed
};

void
MtpConnection::senderClosed (Exception * const exc_,
                             void      * const _self)
{
    MtpConnection * const self = static_cast <MtpConnection*> (_self);

    if (exc_)
        logD_ (_func, "exception: ", exc_->toString());

    if (self->frontend)
        self->frontend.call (self->frontend->closed);
}

void
MtpConnection::sendMediaMessage (Uint32         const stream_id,
                                 MediaMessage * const mt_nonnull msg)
{
    assert (sender);

    Byte header [MOMENT__MTP_CONNECTION__HEADER_SIZE];

    writeBe32 (header + 0, stream_id);

    switch (msg->msg_type) {
        case MediaMessage::Type_Audio:
            header [4] = MessageType::Audio;
            break;
        case MediaMessage::Type_Video:
            header [4] = MessageType::Video;
            break;
        case MediaMessage::Type_MediaDesc:
        case MediaMessage::Type_None:
            unreachable();
    }

    writeBe32 (header +  5, msg->pts_nanosec);
    writeBe32 (header + 13, msg->dts_nanosec);
    writeBe32 (header + 21, msg->msg_len);

    if (msg->msg_type == MediaMessage::Type_Video) {
        VideoMessage * const msg_video = static_cast <VideoMessage*> (msg);
        header [25] = (Byte) msg_video->frame_type;
    } else {
        header [25] = 0;
    }

    Count const num_iovs = 1 + PagePool::countPageListIovs (msg->page_list.first, msg->msg_offset);
    IovArrayElement iov_elems [num_iovs];
    IovArray iovs (iov_elems, num_iovs);
    iovs.addIov (header, sizeof (header));
    PagePool::fillPageListIovs (msg->page_list.first, msg->msg_offset, &iovs);

    sender->sendVector (&iovs, true /* do_flush */);
}

void
MtpConnection::sendMediaDesc (Uint32            const stream_id,
                              MediaDesc_Audio * const media_desc__audio,
                              bool              const set_audio,
                              MediaDesc_Video * const media_desc__video,
                              bool              const set_video)
{
    assert (sender);

    bool const send_audio = set_audio && media_desc__audio;
    bool const send_video = set_video && media_desc__video;

    Byte begin_header [MOMENT__MTP_CONNECTION__HEADER_SIZE];
    {
        writeBe32 (begin_header + 0, stream_id);
        begin_header [4] = MessageType::BeginMediaDesc;
        memset (begin_header + 5, 0, sizeof (begin_header) - 5);
    }

    Byte audio_header [MOMENT__MTP_CONNECTION__HEADER_SIZE];
    PagePool::PageListHead audio_page_list;
    if (send_audio) {
        Size audio_len;
        encodeMediaDesc (media_desc__audio, page_pool, &audio_page_list, &audio_len);

        writeBe32 (audio_header + 0, stream_id);
        audio_header [4] = MessageType::MediaDesc;
        memset (audio_header + 5, 0, 16); // pts, dts
        writeBe32 (audio_header + 21, audio_len);
        audio_header [25] = 0; // frame_type
    }

    Byte video_header [MOMENT__MTP_CONNECTION__HEADER_SIZE];
    PagePool::PageListHead video_page_list;
    if (send_video) {
        Size video_len;
        encodeMediaDesc (media_desc__video, page_pool, &video_page_list, &video_len);

        writeBe32 (video_header + 0, stream_id);
        video_header [4] = MessageType::MediaDesc;
        memset (video_header + 5, 0, 16);
        writeBe32 (video_header + 21, video_len);
        video_header [25] = 0; // frame_type
    }

    Byte end_header [MOMENT__MTP_CONNECTION__HEADER_SIZE];
    {
        writeBe32 (end_header + 0, stream_id);
        end_header [4] = MessageType::EndMediaDesc;
        memset (end_header + 5, 0, sizeof (end_header) - 5);
    }

    Count num_iovs = 2;
    if (send_audio) {
        num_iovs += 1 + PagePool::countPageListIovs (audio_page_list.first, 0 /* msg_offset */);
    }
    if (send_video) {
        num_iovs += 1 + PagePool::countPageListIovs (video_page_list.first, 0 /* msg_offset */);
    }

    IovArrayElement iov_elems [num_iovs];
    IovArray iovs (iov_elems, num_iovs);
    iovs.addIov (begin_header, sizeof (begin_header));
    if (send_audio) {
        iovs.addIov (audio_header, sizeof (audio_header));
        PagePool::fillPageListIovs (audio_page_list.first, 0 /* msg_offset */, &iovs);
    }
    if (send_video) {
        iovs.addIov (video_header, sizeof (video_header));
        PagePool::fillPageListIovs (video_page_list.first, 0 /* msg_offset */, &iovs);
    }
    iovs.addIov (end_header, sizeof (end_header));

    sender->sendVector (&iovs, true /* do_flush */);
}

mt_const void
MtpConnection::init (StreamGroup * const stream_group,
                     Receiver    * const receiver,
                     Sender      * const sender,
                     PagePool    * const mt_nonnull page_pool,
                     CbDesc<Frontend> const &frontend)
{
    this->page_pool = page_pool;
    this->stream_group = stream_group;
    this->frontend = frontend;

    this->receiver = receiver;
    this->sender = sender;

    if (receiver)
        receiver->setFrontend (CbDesc<Receiver::Frontend> (&receiver_frontend, this, this));

    if (sender)
        sender->setFrontend (CbDesc<Sender::Frontend> (&sender_frontend, this, this));
}

MtpConnection::MtpConnection (EmbedContainer * const embed_container)
    : Object             (embed_container),
      in_recv_state      (ReceiveState::MessageHeader),
      in_msg_stream_id   (0),
      in_msg_type        (MessageType::Unknown),
      in_msg_pts_nanosec (0),
      in_msg_dts_nanosec (0),
      in_msg_len         (0),
      in_msg_frame_type  (0),
      in_msg_left        (0),
      in_num_streams     (0)
{
}

mt_sync_domain (receiver) MtpConnection::~MtpConnection ()
{
    page_pool->msgUnref (in_page_list.first);
}

}

