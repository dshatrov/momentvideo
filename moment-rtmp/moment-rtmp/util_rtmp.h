/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__UTIL_RTMP__H__
#define LIBMOMENT__UTIL_RTMP__H__


#include <moment-rtmp/rtmp_connection.h>


namespace Moment {

using namespace M;

namespace RtmpUtil {

Result sendNetStreamPlayPublishNotify (RtmpConnection * mt_nonnull rtmp_conn,
                                       Uint32          msg_stream_id,
                                       ConstMemory     stream_name);

Result sendNetStreamPlayUnpublishNotify (RtmpConnection * mt_nonnull rtmp_conn,
                                         Uint32          msg_stream_id,
                                         ConstMemory     stream_name);

Result sendNetStreamPlayStreamNotFound (RtmpConnection * mt_nonnull rtmp_conn,
                                        Uint32          msg_stream_id,
                                        ConstMemory     stream_name);

Result sendNetStreamPlayStart (RtmpConnection * mt_nonnull rtmp_conn,
                               Uint32          msg_stream_id,
                               ConstMemory     stream_name);

Result sendNetStreamDataStart (RtmpConnection * mt_nonnull rtmp_conn,
                               Uint32          msg_stream_id);

Result sendNetStreamPlayReset (RtmpConnection * mt_nonnull rtmp_conn,
                               Uint32          msg_stream_id,
                               ConstMemory     stream_name);

Result sendRtmpSampleAccess (RtmpConnection * mt_nonnull rtmp_conn,
                             Uint32          msg_stream_id,
                             bool            allow_a,
                             bool            allow_b);

Result sendNetStreamPublishStart (RtmpConnection * mt_nonnull rtmp_conn,
                                  Uint32          msg_stream_id,
                                  ConstMemory     stream_name);

Result sendMetaData (RtmpConnection  * mt_nonnull rtmp_conn,
                     Uint32           msg_stream_id,
                     MediaDesc_Audio *audio_desc,
                     MediaDesc_Video *video_desc);

}

}


#endif /* LIBMOMENT__UTIL_RTMP__H__ */

