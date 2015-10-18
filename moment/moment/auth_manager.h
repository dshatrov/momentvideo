/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__AUTH_MANAGER__H__
#define MOMENT__AUTH_MANAGER__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class AuthManager : public Object
{
  public:
    enum_beg (AuthAction)
        enum_values (Watch, WatchRestream, Stream)
    enum_end (AuthAction)

    typedef void CheckAuthorizationCallback (bool         authorized,
                                             ConstMemory  stream_name,
                                             ConstMemory  restream_reply,
                                             void        *cb_data);

    struct AuthBackend
    {
        bool (*checkAuthorization) (AuthAction     auth_action,
                                    ConstMemory    stream_name,
                                    ConstMemory    auth_key,
                                    IpAddress      client_addr,
                                    CbDesc<CheckAuthorizationCallback> const &cb,
                                    bool          * mt_nonnull ret_authorized,
                                    StRef<String> * mt_nonnull ret_stream_name,
                                    StRef<String> * mt_nonnull ret_restream_reply,
                                    void          *cb_data);

        void (*clientDisconnected) (ConstMemory  auth_key,
                                    IpAddress    client_addr,
                                    ConstMemory  stream_name,
                                    void        *cb_data);
    };

  private:
    mt_const Cb<AuthBackend> auth_backend;

  public:
    mt_const void setAuthBackend (CbDesc<AuthBackend> const &auth_backend)
        { this->auth_backend = auth_backend; }

    bool checkAuthorization (AuthAction     auth_action,
                             ConstMemory    stream_name,
                             ConstMemory    auth_key,
                             IpAddress      client_addr,
                             CbDesc<CheckAuthorizationCallback> const &cb,
                             bool          * mt_nonnull ret_authorized,
                             StRef<String> * mt_nonnull ret_stream_name,
                             StRef<String> * mt_nonnull ret_restream_reply);

    void clientDisconnected (ConstMemory auth_key,
                             IpAddress   client_addr,
                             ConstMemory stream_name);

    AuthManager (EmbedContainer * const embed_container)
        : Object (embed_container)
    {}
};

}


#endif /* MOMENT__AUTH_MANAGER__H__ */

