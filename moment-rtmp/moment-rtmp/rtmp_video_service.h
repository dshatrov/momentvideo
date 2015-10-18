/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__RTMP_VIDEO_SERVICE__H__
#define MOMENT__RTMP_VIDEO_SERVICE__H__


#include <moment-rtmp/rtmp_connection.h>


namespace Moment {

using namespace M;

class RtmpVideoService
{
public:
    struct Frontend {
	Result (*clientConnected) (RtmpConnection  * mt_nonnull rtmp_conn,
				   IpAddress        client_addr,
				   void            *cb_data);
    };

protected:
    mt_const Cb<Frontend> frontend;

public:
    mt_const void setFrontend (CbDesc<Frontend> const &frontend)
        { this->frontend = frontend; }
};

}


#endif /* MOMENT__RTMP_VIDEO_SERVICE__H__ */

