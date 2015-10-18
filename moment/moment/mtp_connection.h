/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__MTP_CONNECTION__H__
#define MOMENT__MTP_CONNECTION__H__


#include <libmary/libmary.h>

#include <moment/av_stream_group.h>


namespace Moment {

using namespace M;

class MtpConnection : public Object
{
private:
    StateMutex mutex;

public:
    struct Frontend
    {
        void (*closed) (void *cb_data);
    };

private:
    class ReceiveState
    {
    public:
        enum Value {
            MessageHeader,
            MessageBody
        };
        operator Value () const { return value; }
        ReceiveState (Value const value) : value (value) {}
        ReceiveState () {}
    private:
        Value value;
    };

    class MessageType
    {
    public:
        enum Value {
            Unknown,
            Audio,
            Video,
            MediaDesc,
            BeginMediaDesc,
            EndMediaDesc
        };
        operator Value () const { return value; }
        MessageType (Value const value) : value (value) {}
        MessageType () {}
    private:
        Value value;
    };

    mt_const Ref<PagePool> page_pool;
    mt_const Ref<StreamGroup> stream_group;
    mt_const Cb<Frontend> frontend;

    mt_const Ref<Receiver> receiver;
    mt_const Ref<Sender>   sender;

    mt_sync_domain (receiver) ReceiveState           in_recv_state;
    mt_sync_domain (receiver) Uint32                 in_msg_stream_id;
    mt_sync_domain (receiver) MessageType            in_msg_type;
    mt_sync_domain (receiver) Uint64                 in_msg_pts_nanosec;
    mt_sync_domain (receiver) Uint64                 in_msg_dts_nanosec;
    mt_sync_domain (receiver) Uint32                 in_msg_len;
    mt_sync_domain (receiver) Byte                   in_msg_frame_type;
    mt_sync_domain (receiver) Uint32                 in_msg_left;
    mt_sync_domain (receiver) bool                   in_media_desc_began;
    mt_sync_domain (receiver) PagePool::PageListHead in_page_list;

    class StreamEntry
    {
    public:
        Uint32 stream_id;
        Ref<MediaDesc_Audio> media_desc__audio;
        Ref<MediaDesc_Video> media_desc__video;
    };

    typedef Map< StreamEntry,
                 MemberExtractor< StreamEntry,
                                  Uint32,
                                  &StreamEntry::stream_id >,
                 DirectComparator<Uint32> >
            StreamMap;

    mt_sync_domain (receiver) StreamMap stream_map;
    mt_sync_domain (receiver) Uint32 in_num_streams;

    mt_sync_domain (receiver) void processInMessage ();

  mt_iface (Receiver::Frontend)
    static Receiver::Frontend const receiver_frontend;

    static mt_sync_domain (receiver) Receiver::ProcessInputResult processInput (Memory  mem,
                                                                                Size   * mt_nonnull ret_accepted,
                                                                                void   *_self);

    static mt_sync_domain (receiver) void processEof (Memory  unprocessed_mem,
                                                      void   *_self);

    static mt_sync_domain (receiver) void processError (Exception *exc_,
                                                        Memory     unprocessed_mem,
                                                        void      *_self);
  mt_iface_end

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderClosed (Exception *exc_,
                              void      *_self);
  mt_iface_end

public:
    void sendMediaMessage (Uint32        stream_id,
                           MediaMessage * mt_nonnull msg);

    void sendMediaDesc (Uint32           stream_id,
                        MediaDesc_Audio *media_desc__audio,
                        bool             set_audio,
                        MediaDesc_Video *media_desc__video,
                        bool             set_video);

    mt_const void init (StreamGroup *stream_group,
                        Receiver    *receiver,
                        Sender      *sender,
                        PagePool    * mt_nonnull page_pool,
                        CbDesc<Frontend> const &frontend);

    MtpConnection (EmbedContainer *embed_container);

//#warning TODO Make sure that MtpConnection is bound to a particular deletion context
    mt_sync_domain (receiver) ~MtpConnection ();
};

}


#endif /* MOMENT__MTP_CONNECTION__H__ */

