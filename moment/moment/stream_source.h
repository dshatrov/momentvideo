/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__STREAM_SOURCE__H__
#define MOMENT__STREAM_SOURCE__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class StreamSource : public Object
{
public:
    struct Frontend {
        void (*error)       (void *cb_data);
        // End of stream.
        void (*eos)         (void *cb_data);
        void (*noVideo)     (void *cb_data);
        void (*gotVideo)    (void *cb_data);
    };

    virtual void releasePipeline () = 0;

    class TrafficStats
    {
    public:
	Uint64 rx_bytes;
	Uint64 rx_audio_bytes;
	Uint64 rx_video_bytes;

	void reset ()
	{
	    rx_bytes = 0;
	    rx_audio_bytes = 0;
	    rx_video_bytes = 0;
	}
    };

    virtual void getTrafficStats (TrafficStats * mt_nonnull ret_traffic_stats) = 0;
    virtual void resetTrafficStats () = 0;

    StreamSource (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__STREAM_SOURCE__H__ */

