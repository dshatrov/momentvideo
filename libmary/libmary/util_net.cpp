/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/types.h>

#include <cstring>
#include <errno.h>
#ifndef LIBMARY_PLATFORM_WIN32
  #include <netdb.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

#include <libmary/log.h>
#include <libmary/cb.h>
#include <libmary/thread.h>
#include <libmary/util_base.h>
#include <libmary/util_str.h>

#include <libmary/util_net.h>


namespace M {

static void stripHostPortWhitespace (ConstMemory * const mt_nonnull ret_mem)
{
    {
        unsigned long n = 0;
        while (n < ret_mem->len()
               && (ret_mem->mem() [0] == ' ' ||
                   ret_mem->mem() [0] == '\t'))
        {
            ++n;
        }

        if (n > 0)
            *ret_mem = ret_mem->region (n);
    }

    {
        unsigned long n = 0;
        while (n < ret_mem->len()
               && (ret_mem->mem() [ret_mem->len() - 1] == ' ' ||
                   ret_mem->mem() [ret_mem->len() - 1] == '\t'))
        {
            ++n;
        }

        if (n > 0)
            *ret_mem = ret_mem->region (0, ret_mem->len() - n);
    }
}

Result splitHostPort (ConstMemory   const hostname,
		      ConstMemory * const ret_host,
		      ConstMemory * const ret_port)
{
    if (ret_host)
	*ret_host = ConstMemory ();
    if (ret_port)
	*ret_port = ConstMemory ();

    void const * const colon = memchr (hostname.mem(), ':', hostname.len());
    if (colon == NULL)
	return Result::Failure;

    if (ret_host) {
	*ret_host = hostname.region (0, (Byte const*) colon - hostname.mem());
        stripHostPortWhitespace (ret_host);
    }

    if (ret_port) {
	*ret_port = hostname.region (((Byte const *) colon - hostname.mem()) + 1);
        stripHostPortWhitespace (ret_port);
    }

    return Result::Success;
}

Result hostToIp (ConstMemory   const host,
		 Uint32      * const ret_addr,
                 bool          const no_dns)
{
    if (host.len() == 0) {
	if (ret_addr)
	    *ret_addr = INADDR_ANY;

	return Result::Success;
    }

    if (ret_addr)
        *ret_addr = 0;

    char host_str [1025];
    if (host.len() >= sizeof (host_str)) {
	logE_ (_func, "host name is too long: ", sizeof (host_str) - 1, " bytes max, got ", host.len(), " bytes: ", host);
	return Result::Failure;
    }
    memcpy (host_str, host.mem(), host.len());
    host_str [host.len()] = 0;

    struct sockaddr_in addr;
#ifndef LIBMARY_PLATFORM_WIN32
    if (!inet_aton (host_str, &addr.sin_addr))
#else
    int addr_len = sizeof (addr.sin_addr);
    if (WSAStringToAddress (host_str, AF_INET, NULL, (struct sockaddr*) &addr, &addr_len))
#endif
    {
        if (no_dns)
            return Result::Failure;

#if defined (LIBMARY_PLATFORM_WIN32) || defined (LIBMARY_PLATFORM_CYGWIN) || defined (__MACH__)
	libraryLock ();

	struct hostent * const he_res = gethostbyname (host_str);
	if (!he_res) {
	    libraryUnlock ();
	    return Result::Failure;
	}

	addr.sin_addr = *(struct in_addr*) he_res->h_addr;

	libraryUnlock ();
#else
	struct hostent he_buf;
	struct hostent *he_res = NULL;
	char he_str_buf_base [1024];
	char *he_str_buf = he_str_buf_base;
	size_t he_str_buf_size = sizeof (he_str_buf_base);
	int herr;
	for (;;) {
	    // TODO: From the manpage:
	    //     POSIX.1-2008 removes the specifications of gethostbyname(), gethostbyaddr(),
	    //     and h_errno, recommending the use of getaddrinfo(3) and getnameinfo(3) instead.
	    int const res = gethostbyname_r (host_str, &he_buf, he_str_buf, he_str_buf_size, &he_res, &herr);
	    if (res == ERANGE) {
		he_str_buf_size <<= 1;
		assert (he_str_buf_size);
		// We don't want the workbuffer to grow larger than 1Mb (a wild guess).
		if (he_str_buf_size > (1 << 20)) {
		    logE_ (_func, "gethostbyname_r(): 1 Mb workbuffer size limit hit");
		    return Result::Failure;
		}

		if (he_str_buf != he_str_buf_base)
		    delete[] he_str_buf;

		he_str_buf = new char [he_str_buf_size];
		assert (he_str_buf);
		continue;
	    }

	    if (res > 0) {
		logE_ (_func, "gethostbyname_r() failed: ", errnoString (res));

		if (he_str_buf != he_str_buf_base)
		    delete[] he_str_buf;

		return Result::Failure;
	    } else
	    if (res != 0) {
		logE_ (_func, "gethostbyname_r(): unexpected return value: ", res);

		if (he_str_buf != he_str_buf_base)
		    delete[] he_str_buf;

		return Result::Failure;
	    }

	    break;
	}
        if (!he_res) {
            logE_ (_func, "could not resolve host name: ", host);
            return Result::Failure;
        }

	addr.sin_addr = *(struct in_addr*) he_res->h_addr;

	if (he_str_buf != he_str_buf_base)
	    delete[] he_str_buf;
#endif
    }

    if (ret_addr)
	*ret_addr = addr.sin_addr.s_addr;

    return Result::Success;
}

#ifdef LIBMARY_MT_SAFE
namespace {
    class HostnameLookup_Data : public Object
    {
      public:
        Mutex mutex;

        mt_const Ref<Timers> timers;
        mt_const Time timeout_microsec;

        mt_const String host;
        mt_const Cb<HostnameLookupCallback> cb;

        mt_mutex (mutex) bool valid;

        HostnameLookup_Data (EmbedContainer * const embed_container)
            : Object           (embed_container),
              timeout_microsec (0),
              valid            (true)
        {}
    };
}

static void hostnameLookup_timerTick (void * const _data)
{
    HostnameLookup_Data * const data = static_cast <HostnameLookup_Data*> (_data);

    data->mutex.lock ();
    if (!data->valid) {
        data->mutex.unlock ();
        return;
    }
    data->valid = false;
    data->mutex.unlock ();

    if (data->cb)
        data->cb.call_ ((IpAddress_NoPort*) NULL);
}

static void hostnameLookup_threadFunc (void * const _data)
{
    HostnameLookup_Data * const data = static_cast <HostnameLookup_Data*> (_data);

    if (data->timers && data->timeout_microsec) {
        data->timers->addTimer_microseconds (CbDesc<Timers::TimerCallback> (hostnameLookup_timerTick, data, data),
                                             data->timeout_microsec,
                                             /*periodical=*/        false,
                                             /*auto_delete=*/       true,
                                             /*delete_after_tick=*/ true);
    }

    IpAddress_NoPort addr ((IpAddress()));
    // blocking call
    Result const res = hostToIp (data->host, &addr.ip_addr);

    data->mutex.lock ();
    if (!data->valid) {
        data->mutex.unlock ();
        return;
    }
    data->valid = false;
    data->mutex.unlock ();

    if (res) {
        if (data->cb)
            data->cb.call_ (&addr);
    } else {
        if (data->cb)
            data->cb.call_ ((IpAddress_NoPort*) NULL);
    }
}
#endif

bool hostnameLookup_async (ConstMemory                            const host,
                           CbDesc<HostnameLookupCallback> const &cb,
                           IpAddress_NoPort                     * const mt_nonnull ret_addr,
                           Result                               * const mt_nonnull ret_res,
                           Timers                               * const timers,
                           Time                                   const timeout_microsec)
{
    if (hostToIp (host, &ret_addr->ip_addr, /*no_dns=*/ true)) {
        *ret_res = Result::Success;
        return true;
    }

  #ifdef LIBMARY_MT_SAFE
    Ref<HostnameLookup_Data> const data = grabNewObject <HostnameLookup_Data> ();
    data->timers           = timers;
    data->timeout_microsec = timeout_microsec;
    data->host             = host;
    data->cb               = cb;

    Ref<Thread> const thread = grabNewObject <Thread> (
            CbDesc<Thread::ThreadFunc> (hostnameLookup_threadFunc, data, NULL, data));
    thread->spawn (/*joinable=*/ false);

    ret_addr->ip_addr = 0;
    *ret_res = Result::Success;
    return false;
  #else
    Uint32 ip_addr;
    *ret_res = hostToIp (host, &ip_addr);
    ret_addr->ip_addr = ip_addr;
    return true;
  #endif
}

Result serviceToPort (ConstMemory   const service,
		      Uint16      * const ret_port)
{
    char service_str [1025];
    if (service.len() >= sizeof (service_str)) {
	logE_ (_func, "service name is too long: ", sizeof (service_str) - 1, " bytes max, got ", service.len(), " bytes: ", service);
	return Result::Failure;
    }
    memcpy (service_str, service.mem(), service.len());
    service_str [service.len()] = 0;

    char *endptr;
    Uint16 port = (Uint16) strtoul (service_str, &endptr, 0);
    if (*endptr != 0) {
#if    defined LIBMARY_PLATFORM_ANDROID \
    || defined LIBMARY_PLATFORM_WIN32   \
    || defined LIBMARY_PLATFORM_CYGWIN  \
    || defined __MACH__
	libraryLock ();

        struct servent * const se_res = getservbyname (service_str, "tcp");
        if (!se_res) {
            libraryUnlock ();
            return Result::Failure;
        }

	port = se_res->s_port;

	libraryUnlock ();
#else
	struct servent se_buf;
	struct servent *se_res = NULL;
	char se_str_buf_base [1024];
	char *se_str_buf = se_str_buf_base;
	size_t se_str_buf_size = sizeof (se_str_buf_base);
	for (;;) {
	    int const res = getservbyname_r (service_str, "tcp", &se_buf, se_str_buf, se_str_buf_size, &se_res);
	    if (res == ERANGE) {
		se_str_buf_size <<= 1;
		assert (se_str_buf_size);
		// We don't want the workbuffer to grow larger than 1Mb (a wild guess).
		if (se_str_buf_size > (1 << 20)) {
		    logE_ (_func, "getservbyname_r(): 1 Mb workbuffer size limit hit");
		    return Result::Failure;
		}

		if (se_str_buf != se_str_buf_base)
		    delete[] se_str_buf;

		se_str_buf = new char [se_str_buf_size];
		assert (se_str_buf);
		continue;
	    }

	    if (res > 0) {
		logE_ (_func, "getservbyname_r() failed: ", errnoString (res));

		if (se_str_buf != se_str_buf_base)
		    delete[] se_str_buf;

		return Result::Failure;
	    } else
	    if (res != 0) {
		logE_ (_func, "getservbyname_r(): unexpected return value: ", res);

		if (se_str_buf != se_str_buf_base)
		    delete[] se_str_buf;

		return Result::Failure;
	    }

	    break;
	}
	assert (se_res);

	port = se_res->s_port;

	if (se_str_buf != se_str_buf_base)
	    delete[] se_str_buf;
#endif
    }

    if (ret_port)
	*ret_port = port;

    return Result::Success;
}

Size
IpAddress::toString_ (Memory const &mem,
		      Format const & /* fmt */) const
{
    Uint32 const addr_in_host_byte_order = ntohl (ip_addr);
    Size offs = 0;
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >> 24) & 0xff, fmt_def);
    offs += toString (mem.safeRegion (offs), ".");
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >> 16) & 0xff, fmt_def);
    offs += toString (mem.safeRegion (offs), ".");
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >>  8) & 0xff, fmt_def);
    offs += toString (mem.safeRegion (offs), ".");
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >>  0) & 0xff, fmt_def);
    offs += toString (mem.safeRegion (offs), ":");
    offs += toString (mem.safeRegion (offs), port);
    return offs;
}

Size
IpAddress_NoPort::toString_ (Memory const &mem,
                             Format const & /* fmt */) const
{
    Uint32 const addr_in_host_byte_order = ntohl (ip_addr);
    Size offs = 0;
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >> 24) & 0xff, fmt_def);
    offs += toString (mem.safeRegion (offs), ".");
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >> 16) & 0xff, fmt_def);
    offs += toString (mem.safeRegion (offs), ".");
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >>  8) & 0xff, fmt_def);
    offs += toString (mem.safeRegion (offs), ".");
    offs += toString (mem.safeRegion (offs), (addr_in_host_byte_order >>  0) & 0xff, fmt_def);
    return offs;
}

Result setIpAddress_default (ConstMemory   const &hostname,
			     ConstMemory   const &default_host,
			     Uint16        const &default_port,
			     bool          const  allow_any_host,
			     IpAddress   * const  ret_addr)
{
    if (hostname.isNull()) {
      // Empty hostname (without a colon).
	logE_ (_func, "empty hostname");
	return Result::Failure;
    }

    ConstMemory host;
    ConstMemory port;
    if (!splitHostPort (hostname, &host, &port)) {
      // @hostname contains only host part. Using default port number.
	return setIpAddress (hostname, default_port, ret_addr);
    }

    if (host.isNull() && !allow_any_host) {
	logE_ (_func, "empty host");
	return Result::Failure;
    }

    if (port.isNull()) {
	if (host.isNull()) {
	  // ":" - default address case.
	    return setIpAddress (default_host, default_port, ret_addr);
	}

	return setIpAddress (host, default_port, ret_addr);
    }

    return setIpAddress (host, port, ret_addr);
}

// Note: 'ip_addr' is in network byte order to allow the use of INADDR_ANY
// constant wihout changing its byte order. 'port' is in host byte order.
//
void setIpAddress (Uint32 const ip_addr,
		   Uint16 const port,
		   struct sockaddr_in * const ret_addr)
{
    if (ret_addr) {
	memset (ret_addr, 0, sizeof (*ret_addr));
	ret_addr->sin_family = AF_INET;
	ret_addr->sin_addr.s_addr = ip_addr;
	ret_addr->sin_port = htons (port);
    }
}

void setIpAddress (struct sockaddr_in * const mt_nonnull addr,
		   IpAddress          * const mt_nonnull ret_addr)
{
    ret_addr->ip_addr = addr->sin_addr.s_addr;
    ret_addr->port = ntohs (addr->sin_port);
}

Result
parseUri (ConstMemory   const uri,
          Uint16        const default_port,
          ConstMemory * const ret_proto,
          ConstMemory * const ret_login,
          ConstMemory * const ret_password,
          ConstMemory * const ret_host,
          Uint16      * const ret_port,
          ConstMemory * const ret_path)
{
    if (ret_proto)
        *ret_proto = ConstMemory();

    if (ret_login)
        *ret_login = ConstMemory();

    if (ret_password)
        *ret_password = ConstMemory();

    if (ret_host)
        *ret_host = ConstMemory();

    if (ret_port)
        *ret_port = 0;

    if (ret_path)
        *ret_path = ConstMemory();

  // URI forms:   rtmp://user:password@host:port/foo/bar
  //              rtmp://host:port/foo/bar

    Size pos = 0;

    while (pos < uri.len() && uri.buf() [pos] != ':')
        ++pos;

    ConstMemory const proto = uri.region (0, pos);
    if (ret_proto)
        *ret_proto = proto;

    if (pos < uri.len())
        ++pos;

    for (unsigned i = 0; i < 2; ++i) {
        while (pos < uri.len()) {
            if (uri.buf() [pos] == '/') {
                ++pos;
                break;
            }
            ++pos;
        }
    }

    // user:password@host:port
    Size const user_addr_begin = pos;

    while (pos < uri.len() && uri.buf() [pos] != '/')
        ++pos;

    ConstMemory const user_addr = uri.region (user_addr_begin, pos - user_addr_begin);
    Size at_pos = 0;
    bool got_at = false;
    while (at_pos < user_addr.len()) {
        if (user_addr.buf() [at_pos] == '@') {
            got_at = true;
            break;
        }
        ++at_pos;
    }

    ConstMemory addr_mem = user_addr;
    if (got_at) {
        Size semicolon_pos = 0;
        ConstMemory const login_password = user_addr.region (0, at_pos);
        while (semicolon_pos < login_password.len() && login_password.buf() [semicolon_pos] != ':')
            ++semicolon_pos;

        if (semicolon_pos < login_password.len()) {
            if (ret_login)
                *ret_login = login_password.region (0, semicolon_pos);

            if (ret_password)
                *ret_password = login_password.region (semicolon_pos + 1);
        }

        addr_mem = user_addr.region (at_pos + 1, user_addr.len() - (at_pos + 1));
    }

    {
        Size semicolon_pos = 0;
        while (semicolon_pos < addr_mem.len() && addr_mem.buf() [semicolon_pos] != ':')
            ++semicolon_pos;

        if (semicolon_pos < addr_mem.len()) {
            if (ret_host)
                *ret_host = addr_mem.region (0, semicolon_pos);

            Uint32 port = default_port;
            ConstMemory const port_mem = addr_mem.region (semicolon_pos + 1);
            if (!strToUint32_safe (port_mem, &port, /*base=*/ 10)
                || (Uint32) (Uint16) port != port)
            {
                logD_ (_func, "bad port \"", port_mem, "\" in uri \"", uri, "\"");
                return Result::Failure;
            }

            if (ret_port)
                *ret_port = (Uint16) port;
        } else {
            if (ret_host)
                *ret_host = addr_mem;

            if (ret_port)
                *ret_port = default_port;
        }
    }

    if (pos < uri.len())
        ++pos;

    if (ret_path)
        *ret_path = uri.region (pos, uri.len() - pos);

    return Result::Success;
}

}

