#ifndef MOMENT__FETCH_CONNECTION__H__
#define MOMENT__FETCH_CONNECTION__H__


#include <moment/moment_types.h>


namespace Moment {

class FetchConnection : public Object
{
  public:
    struct FetchConnectionFrontend
    {
        typedef void Disconnected (DisconnectReason  disconnect_reason,
                                   void             *cb_data);

        typedef void GotVideo (bool  got_video,
                               void *cb_data);

        Disconnected *disconnected;
        GotVideo     *gotVideo;

        constexpr FetchConnectionFrontend (Disconnected * const disconnected,
                                           GotVideo     * const gotVideo)
            : disconnected (disconnected),
              gotVideo     (gotVideo)
        {}
    };

    // Should be called only once.
    virtual Result start () = 0;

    FetchConnection (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};


}


#endif /* MOMENT__FETCH_CONNECTION__H__ */

