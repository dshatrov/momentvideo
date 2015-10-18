/*  Copyright (C) 2013-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_moment.h>


namespace Moment {

Result
parseMomentUri (ConstMemory   const uri,
                Uint16        const default_port,
                IpAddress   * const mt_nonnull ret_server_addr,
                ConstMemory * const mt_nonnull ret_app_name,
                ConstMemory * const mt_nonnull ret_stream_name,
                bool        * const mt_nonnull ret_momentrtmp_proto,
                ConstMemory * const ret_login,
                ConstMemory * const ret_password,
                ConstMemory * const ret_proto,
                Size        * const ret_loginpass_beg,
                Size        * const ret_loginpass_end)
{
    *ret_momentrtmp_proto = false;

    if (ret_login)
        *ret_login = ConstMemory();

    if (ret_password)
        *ret_password = ConstMemory ();

    if (ret_loginpass_beg)
        *ret_loginpass_beg = 0;
    if (ret_loginpass_end)
        *ret_loginpass_end = 0;

  // URI forms:   rtmp://user:password@host:port/foo/bar
  //              rtmp://host:port/foo/bar
  //
  // Note that we do not extract user:password from the URI but rather
  // accept them as separate function parameters. This is inconsistent.
  // It might be convenient to parse user:password and use that
  // instead of explicit parameters when the latter is null.

    unsigned long pos = 0;

    while (pos < uri.len()) {
        if (uri.mem() [pos] == ':') {
            ++pos;
            break;
        }
        ++pos;
    }

    ConstMemory const proto = uri.region (0, (pos >= 1 ? pos - 1 : 0));
    if (ret_proto)
        *ret_proto = proto;

    if (equal (proto, "momentrtmp"))
        *ret_momentrtmp_proto = true;

    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/') {
            ++pos;
            break;
        }
        ++pos;
    }

    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/') {
            ++pos;
            break;
        }
        ++pos;
    }

    // user:password@host:port
    unsigned long const user_addr_begin = pos;

    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/')
            break;
        ++pos;
    }

    ConstMemory const user_addr = uri.region (user_addr_begin, pos - user_addr_begin);
    unsigned long at_pos = 0;
    bool got_at = false;
    while (at_pos < user_addr.len()) {
        if (user_addr.mem() [at_pos] == '@') {
            got_at = true;
            break;
        }
        ++at_pos;
    }

    ConstMemory addr_mem = user_addr;
    if (got_at) {
        Size semicolon_pos = 0;
        ConstMemory const login_password = user_addr.region (0, at_pos);
        while (semicolon_pos < login_password.len() && login_password.mem() [semicolon_pos] != ':')
            ++semicolon_pos;

        if (semicolon_pos < login_password.len()) {
            if (ret_login)
                *ret_login = login_password.region (0, semicolon_pos);

            if (ret_password)
                *ret_password = login_password.region (semicolon_pos + 1);

            if (ret_loginpass_beg)
                *ret_loginpass_beg = login_password.buf() - uri.buf();

            if (ret_loginpass_end)
                *ret_loginpass_end = (login_password.buf() - uri.buf()) + login_password.len() + 1;
        }

        addr_mem = user_addr.region (at_pos + 1, user_addr.len() - (at_pos + 1));
    }

    if (!setIpAddress_default (addr_mem,
                               ConstMemory() /* default_host */,
                               default_port,
                               false          /* allow_any_host */,
                               ret_server_addr))
    {
        logE_ (_func, "could not extract address from uri: ", uri);
        return Result::Failure;
    }

    if (pos < uri.len())
        ++pos;

    unsigned long const app_name_begin = pos;
    while (pos < uri.len()) {
        if (uri.mem() [pos] == '/')
            break;
        ++pos;
    }

    *ret_app_name = uri.region (app_name_begin, pos - app_name_begin);

    if (pos < uri.len())
        ++pos;

    *ret_stream_name = uri.region (pos, uri.len() - pos);

    return Result::Success;
}

ConstMemory extractDomainFromUrl (ConstMemory url)
{
    url = stripWhitespace (url);

    // Examples of possible url forms:
    //   domain.com
    //   domain.com/foo/bar
    //   domain.com:1935
    //   domain.com:1935/foo/bar
    //   rtmp://domain.com
    //   rtmp://domain.com:1935
    //   http://domain.com/foo/bar
    //   http://domain.com:8080/foo/bar

    Byte const * const dot_ptr = (Byte*) memchr (url.mem(), ':', url.len());

    if (dot_ptr
        && url.len() - (dot_ptr - url.mem()) >= 2
        && dot_ptr [1] == '/'
        && dot_ptr [2] == '/')
    {
        url = url.region ((dot_ptr - url.mem()) + 3);
    }

    {
        Size i = 0;
        while (i < url.len()) {
            if (url.mem() [i] == ':' || url.mem() [i] == '/')
                break;

            ++i;
        }

        url = url.region (0, i);
    }

    return url;
}

static bool matchDomainMask (ConstMemory const domain,
                             ConstMemory const mask)
{
    if (mask.len() == 0 || domain.len() == 0)
        return false;

    Size i = mask.len();
    Size j = domain.len();
    for (;;) {
        if (mask.mem() [i - 1] == '*') {
            while (domain.mem() [j - 1] != '.') {
                --j;
                if (j == 0)
                    break;
            }
        } else {
            if (mask.mem() [i - 1] != domain.mem() [j - 1])
                return false;

            --j;
        }

        --i;

        if (i == 0 && j == 0)
            return true;

        if (i == 0 || j == 0)
            break;
    }

    return false;
}

bool isAllowedDomain (ConstMemory   const domain,
                      DomainList  * const mt_nonnull allowed_domains)
{
    if (allowed_domains->isEmpty())
        return true;

    DomainList::iterator iter (*allowed_domains);
    while (!iter.done()) {
        StRef<String> &str = iter.next()->data;
        if (matchDomainMask (domain, str->mem()))
            return true;
    }

    return false;
}

}

