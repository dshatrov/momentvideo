/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__UTIL_NET__H__
#define LIBMARY__UTIL_NET__H__


#include <libmary/types.h>

#ifndef LIBMARY_PLATFORM_WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <libmary/timers.h>
#include <libmary/log.h>


namespace M {

Result splitHostPort (ConstMemory  hostname,
		      ConstMemory *ret_host,
		      ConstMemory *ret_port);

Result hostToIp (ConstMemory  host,
                 Uint32      *ret_addr,
                 bool         no_dns = false);

Result serviceToPort (ConstMemory  service,
		      Uint16      *ret_port);


// _______________________ Arch-independent setIpAddress _______________________

    class IpAddress
    {
    public:
        // Note: 'ip_addr' is in network byte order to allow the use of INADDR_ANY
        // constant wihout changing its byte order. 'port' is in host byte order.
	Uint32 ip_addr;
	Uint16 port;

        bool operator == (IpAddress const &addr) const
            { return (ip_addr == addr.ip_addr) && (port == addr.port); }

        bool operator != (IpAddress const &addr) const
            { return !(*this == addr); }

	void reset ()
	{
	    ip_addr = 0;
	    port = 0;
	}

	Size toString_ (Memory const &mem,
		        Format const &fmt) const;

        IpAddress ()
            : ip_addr (0),
              port (0)
        {}
    };

    class IpAddress_NoPort
    {
    public:
        Uint32 ip_addr;

        Size toString_ (Memory const &mem,
                        Format const &fmt) const;

        IpAddress_NoPort (IpAddress const addr)
            : ip_addr (addr.ip_addr)
        {}
    };

    static inline void setIpAddress (Uint32 const ip_addr,
				     Uint16 const port,
				     IpAddress * const ret_addr)
    {
	if (ret_addr) {
	    ret_addr->ip_addr = ip_addr;
	    ret_addr->port = port;
	}
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       IpAddress *ret_addr);

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 port,
				       IpAddress *ret_addr);


    static inline Result setIpAddress (ConstMemory const &hostname,
				       IpAddress * const ret_addr)
    {
	ConstMemory host;
	ConstMemory port;
	if (!splitHostPort (hostname, &host, &port)) {
	    logE_ (_func, "no colon found in hostname: ", hostname);
	    return Result::Failure;
	}

	return setIpAddress (host, port, ret_addr);
    }

    // @allow_any - If 'true', then set host to INADDR_ANY when @hostname is empty.
    Result setIpAddress_default (ConstMemory const &hostname,
				 ConstMemory const &default_host,
				 Uint16      const &default_port,
				 bool               allow_any_host,
				 IpAddress         *ret_addr);

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       IpAddress * const ret_addr)
    {
	Uint16 port;
	if (!serviceToPort (service, &port))
	    return Result::Failure;

	return setIpAddress (host, port, ret_addr);
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 const port,
				       IpAddress * const ret_addr)
    {
	Uint32 ip_addr;
	if (!hostToIp (host, &ip_addr))
	    return Result::Failure;

	setIpAddress (ip_addr, port, ret_addr);
	return Result::Success;
    }


// _________________________ Arch-specific setIpAddress ________________________

    void setIpAddress (Uint32 ip_addr,
		       Uint16 port,
		       struct sockaddr_in *ret_addr);

    void setIpAddress (struct sockaddr_in * mt_nonnull addr,
		       IpAddress          * mt_nonnull ret_addr);

    static inline void setIpAddress (IpAddress const &addr,
				     struct sockaddr_in * const ret_addr)
        { return setIpAddress (addr.ip_addr, addr.port, ret_addr); }

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       struct sockaddr_in *ret_addr);

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 port,
				       struct sockaddr_in *ret_addr);

    static inline Result setIpAddress (ConstMemory const &hostname,
				       struct sockaddr_in * const ret_addr)
    {
	ConstMemory host;
	ConstMemory port;
	if (!splitHostPort (hostname, &host, &port)) {
	    logE_ (_func, "no colon found in hostname: ", hostname);
	    return Result::Failure;
	}

	return setIpAddress (host, port, ret_addr);
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       ConstMemory const &service,
				       struct sockaddr_in * const ret_addr)
    {
	Uint16 port;
	if (!serviceToPort (service, &port))
	    return Result::Failure;

	return setIpAddress (host, port, ret_addr);
    }

    static inline Result setIpAddress (ConstMemory const &host,
				       Uint16 const port,
				       struct sockaddr_in * const ret_addr)
    {
	Uint32 ip_addr;
	if (!hostToIp (host, &ip_addr))
	    return Result::Failure;

	setIpAddress (ip_addr, port, ret_addr);
	return Result::Success;
    }

// _____________________________________________________________________________


typedef void HostnameLookupCallback (IpAddress_NoPort *addr,
                                     void             *cb_data);

bool hostnameLookup_async (ConstMemory                           host,
                           CbDesc<HostnameLookupCallback> const &cb,
                           IpAddress_NoPort                     * mt_nonnull ret_addr,
                           Result                               * mt_nonnull ret_res,
                           Timers                               *timers = NULL,
                           Time                                  timeout_microsec = 0);

Result parseUri (ConstMemory  uri,
                 Uint16       default_port,
                 ConstMemory *ret_proto,
                 ConstMemory *ret_login,
                 ConstMemory *ret_password,
                 ConstMemory *ret_host,
                 Uint16      *ret_port,
                 ConstMemory *ret_path);

}


#endif /* LIBMARY__UTIL_NET__H__ */

