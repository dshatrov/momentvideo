/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__FLV_MUXER__H__
#define MOMENT__FLV_MUXER__H__


#include <moment/av_muxer.h>


namespace Moment {

mt_unsafe class FlvMuxer : public AvMuxer
{
  private:
    PagePool *page_pool;

    bool got_first_timestamp;
    Uint64 first_timestamp_nanosec;
    Uint64 last_timestamp_nanosec;

    void muxMetadata ();

    void doMuxMessage (MediaMessage * mt_nonnull msg,
		       Byte          msg_type,
                       ConstMemory   msg_header);

  public:
    mt_throws Result beginMuxing ();
    mt_throws Result endMuxing   ();

    mt_throws Result muxAudioMessage (AudioMessage * mt_nonnull msg);
    mt_throws Result muxVideoMessage (VideoMessage * mt_nonnull msg);

    void flush ();

    void setPagePool (PagePool * const page_pool) { this->page_pool = page_pool; }

    FlvMuxer ();
};

}


#endif /* MOMENT__FLV_MUXER__H__ */

