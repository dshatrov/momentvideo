/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__TRANSCODING_MANAGER__H__
#define MOMENT__TRANSCODING_MANAGER__H__


#include <moment/transcoder.h>


namespace Moment {

using namespace M;

class TranscodingManager : public Object
{
  private:
    StateMutex mutex;

  public:
    struct TranscoderBackend
    {
        Ref<Transcoder> (*newTranscoder) (void *cb_data);
    };

  private:
    mt_mutex (mutex) Cb<TranscoderBackend> transcoder_backend;

  public:
    void setTranscoderBackend (CbDesc<TranscoderBackend> const &cb)
    {
        mutex.lock ();
        transcoder_backend = cb;
        mutex.unlock ();
    }

    Ref<Transcoder> newTranscoder ()
    {
        mutex.lock ();
        Cb<TranscoderBackend> const cb = transcoder_backend;
        mutex.unlock ();

        Ref<Transcoder> transcoder;
        if (cb)
            cb.call_ret (&transcoder, cb->newTranscoder);

        return transcoder;
    }

    TranscodingManager (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__TRANSCODING_MANAGER__H__ */

