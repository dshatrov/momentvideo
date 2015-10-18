/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__UTIL_MOMENT__H__
#define MOMENT__UTIL_MOMENT__H__


#include <libmary/libmary.h>

#include <moment/domain_options.h>


namespace Moment {

using namespace M;

Result parseMomentUri (ConstMemory  uri,
                       Uint16       default_port,
                       IpAddress   * mt_nonnull ret_server_addr,
                       ConstMemory * mt_nonnull ret_app_name,
                       ConstMemory * mt_nonnull ret_stream_name,
                       bool        * mt_nonnull ret_momentrtmp_proto,
                       ConstMemory *ret_login         = NULL,
                       ConstMemory *ret_password      = NULL,
                       ConstMemory *ret_proto         = NULL,
                       Size        *ret_loginpass_beg = NULL,
                       Size        *ret_loginpass_end = NULL);

ConstMemory extractDomainFromUrl (ConstMemory url);

bool isAllowedDomain (ConstMemory  domain,
                      DomainList  * mt_nonnull allowed_domains);

}


#endif /* MOMENT__UTIL_MOMENT__H__ */

