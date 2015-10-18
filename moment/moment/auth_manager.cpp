/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/auth_manager.h>


namespace Moment {

bool
AuthManager::checkAuthorization (AuthAction      const auth_action,
                                 ConstMemory     const stream_name,
                                 ConstMemory     const auth_key,
                                 IpAddress       const client_addr,
                                 CbDesc<CheckAuthorizationCallback> const &cb,
                                 bool          * const mt_nonnull ret_authorized,
                                 StRef<String> * const mt_nonnull ret_stream_name,
                                 StRef<String> * const mt_nonnull ret_restream_reply)
{
    *ret_authorized = false;
    *ret_stream_name = NULL;
    *ret_restream_reply = NULL;

    if (!auth_backend) {
        *ret_authorized = true;
        return true;
    }

    bool complete = false;
    bool authorized = false;
    if (!auth_backend.call_ret<bool> (&complete,
                                      auth_backend->checkAuthorization,
                                      /*(*/
                                          auth_action,
                                          stream_name,
                                          auth_key,
                                          client_addr,
                                          cb,
                                          &authorized,
                                          ret_stream_name,
                                          ret_restream_reply
                                      /*)*/))
    {
        logW_ ("authorization backend gone");
        *ret_authorized = true;
        return true;
    }

    if (!complete)
        return false;

    *ret_authorized = authorized;
    return true;
}

void AuthManager::clientDisconnected (ConstMemory const auth_key,
                                      IpAddress   const client_addr,
                                      ConstMemory const stream_name)
{
    if (auth_backend) {
        auth_backend.call (auth_backend->clientDisconnected,
                           /*(*/ auth_key, client_addr, stream_name /*)*/);
    }
}

}

