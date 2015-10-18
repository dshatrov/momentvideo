/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMOMENT__MOMENT_TYPES__H__
#define LIBMOMENT__MOMENT_TYPES__H__


#include <libmary/types.h>

#ifndef LIBMARY_XCODE_BUILD
  #include <moment/libmoment_config.h>
#endif


namespace Moment {

using namespace M;

enum_beg (DisconnectReason)
    enum_values (
        InvalidReason,
        CouldNotEstablishUnderlyingConnection,
        UnderlyingConnectionTerminated,
        SendQueueOverflow,
        CommunicationTimeout,
        ProtocolError,
        ResourceNotFound,
        AccessDenied,
        LocalClose,
        RemoteClose,
        TodoCategorizeMe
    )
enum_end (DisconnectReason)

enum_beg (RecordingMode)
    enum_values (
        NoRecording,
        Replace,
        Append
    )
enum_end (RecordingMode)

}


#endif /* LIBMOMENT__MOMENT_TYPES__H__ */

