/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__AV_MUXER__H__
#define MOMENT__AV_MUXER__H__


#include <moment/media_message.h>


namespace Moment {

mt_unsafe class AvMuxer
{
  protected:
    Sender *sender;

  public:
    virtual mt_throws Result beginMuxing () = 0;
    virtual mt_throws Result muxAudioMessage (AudioMessage * mt_nonnull msg) = 0;
    virtual mt_throws Result muxVideoMessage (VideoMessage * mt_nonnull msg) = 0;
    virtual mt_throws Result endMuxing () = 0;

    void setSender (Sender * const sender) { this->sender = sender; }

    AvMuxer () : sender (NULL) {}

    virtual ~AvMuxer () {}
};

}


#endif /* MOMENT__AV_MUXER__H__ */

