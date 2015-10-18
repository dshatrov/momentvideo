/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/util_moment.h>

#include <moment/config_manager.h>


namespace Moment {
    
mt_one_of(( mt_const, mt_mutex (mutex) )) void
ConfigManager::setOverridableVar (ConstMemory     const var_name,
                                  ConstMemory     const opt_name,
                                  ConstMemory     const host,
                                  Uint32          const port,
                                  StRef<String> * const mt_nonnull ret_preset)
{
    if (VarHashEntry * const entry = default_var_hash.lookup (var_name)) {
        *ret_preset = st_grab (new (std::nothrow) String (entry->var->getValue()));
    } else {
        ConstMemory opt_val = config->getString (opt_name);
        if (opt_val.len())
            logI_ (_func, opt_name, ":  ", opt_val);

        StRef<String> hostport_str;
        if (!opt_val.len()
            && host.len())
        {
            hostport_str = makeString (host, ":", port);
            opt_val = hostport_str->mem();
        }

        if (opt_val.len()) {
            *ret_preset = st_grab (new (std::nothrow) String (opt_val));

            default_varlist->addEntry (var_name,
                                       opt_val,
                                       true  /* with_value */,
                                       false /* enable_section */,
                                       false /* disable_section */);

            VarHashEntry * const entry = new (std::nothrow) VarHashEntry;
            assert (entry);
            entry->var = default_varlist->var_list.getLast();

            default_var_hash.add (entry);
        }
    }
}

static void
setBindPort (MConfig::Config * const mt_nonnull config,
             ConstMemory       const opt_name,
             Uint32            const default_port,
             Uint32          * const mt_nonnull ret_port)
{
    ConstMemory const bind_mem = config->getString_default (opt_name, ConstMemory());
    if (bind_mem.len()) {
        IpAddress addr;
        if (!setIpAddress_default (bind_mem,
                                   ConstMemory() /* default_host */,
                                   default_port,
                                   true          /* allow_any_host */,
                                   &addr))
        {
            logE_ (_func, "setIpAddress_default() failed (", opt_name, ")");
            *ret_port = default_port;
            return;
        }

        *ret_port = addr.port;
    } else {
        *ret_port = default_port;
    }
}

mt_one_of(( mt_const, mt_mutex (mutex) )) void
ConfigManager::parseDefaultVarlist (MConfig::Config * mt_nonnull const new_config)
{
    releaseDefaultVarHash ();

    {
        Ref<MConfig::Varlist> const new_varlist = grab (new (std::nothrow) MConfig::Varlist (NULL /* embed_container */));
        {
            MConfig::Section * const varlist_section = new_config->getSection ("moment/vars", false /* create */);
            if (varlist_section)
                MConfig::parseVarlistSection (varlist_section, new_varlist);
        }

        default_varlist = new_varlist;
        {
            MConfig::Varlist::VarList::iterator iter (new_varlist->var_list);
            while (!iter.done()) {
                MConfig::Varlist::Var * const var = iter.next ();
                VarHashEntry * const entry = new (std::nothrow) VarHashEntry;
                assert (entry);
                entry->var = var;

                if (VarHashEntry * const old_entry = default_var_hash.lookup (var->getName())) {
                    default_var_hash.remove (old_entry);
                    delete old_entry;
                }

                default_var_hash.add (entry);
            }
        }
    }

    {
        setBindPort (config, "mod_rtmp/rtmp_bind",  1935, &rtmp_bind_port);
        setBindPort (config, "mod_rtmp/rtmpt_bind", 0,    &rtmpt_bind_port);
        setBindPort (config, "http/http_bind",      8080, &http_bind_port);
        setBindPort (config, "http/admin_bind",     8082, &admin_bind_port);
        setBindPort (config, "mod_rtsp/rtsp_bind",  5554, &rtsp_bind_port);

        hls_bind_port = http_bind_port;

        if (rtmpt_bind_port == 0)
            rtmpt_bind_port = http_bind_port;

        logD_ (_func, "rtmp_bind_port:  ", rtmp_bind_port);
        logD_ (_func, "rtmpt_bind_port: ", rtmpt_bind_port);
        logD_ (_func, "http_bind_port:  ", http_bind_port);
        logD_ (_func, "admin_bind_port: ", admin_bind_port);
        logD_ (_func, "rtsp_bind_port:  ", rtsp_bind_port);
    }

    ConstMemory host;
    {
        ConstMemory const opt_name = "moment/host";
        host = config->getString_default (opt_name, ConstMemory());
    }

    setOverridableVar ("RtmpAddr",  "moment/this_rtmp_server_addr",  host, rtmp_bind_port,  &rtmp_addr_preset);
    setOverridableVar ("RtmptAddr", "moment/this_rtmpt_server_addr", host, rtmpt_bind_port, &rtmpt_addr_preset);
    setOverridableVar ("HlsAddr",   "moment/this_hls_server_addr",   host, hls_bind_port,   &hls_addr_preset);
    setOverridableVar ("RtspAddr",  "moment/this_rtsp_server_addr",  host, rtsp_bind_port,  &rtsp_addr_preset);
    setOverridableVar ("HttpAddr",  "moment/this_http_server_addr",  host, http_bind_port,  &http_addr_preset);
    setOverridableVar ("AdminAddr", "moment/this_admin_server_addr", host, admin_bind_port, &admin_addr_preset);
}

mt_mutex (mutex) void
ConfigManager::releaseDefaultVarHash ()
{
    VarHash::iterator iter (default_var_hash);
    while (!iter.done()) {
        VarHashEntry * const entry = iter.next ();
        delete entry;
    }

    default_var_hash.clear ();
}

Ref<MConfig::Config>
ConfigManager::getConfig ()
{
    mutex.lock ();
    Ref<MConfig::Config> const tmp_config = config;
    mutex.unlock ();
    return tmp_config;
}

Ref<MConfig::Varlist>
ConfigManager::getDefaultVarlist ()
{
    mutex.lock ();
    Ref<MConfig::Varlist> const tmp_varlist = default_varlist;
    mutex.unlock ();
    return tmp_varlist;
}

void
ConfigManager::setConfig (MConfig::Config * mt_nonnull const new_config)
{
    mutex.lock ();
    config = new_config;
    parseDefaultVarlist (new_config);

    // TODO Simplify synchronization: require the client to call getConfig() when handling configReload().
    //      'mutex' will then become unnecessary (! _vast_ simplification).
    //      Pass MomentServer pointer arg for convenience.
    // ^^^ The idea may be not that good at all.

    // setConfig() может вызываться параллельно
    // каждый setConfig() завершится только после уведомления всех слушателей
    // без mutex не будет установленного порядка уведомления
    // вызовы setConfig() могут быть вложенными, в этом случае параметр new_config не работает.
    // два параллельных вызова setConfig => путаница, какой конфиг реально в силе?
    // НО при этом реально в силе только один конфиг.
    // Я за то, чтобы _явно_ сериализовать обновления конфига и не накладывать лишний ограничений.
    // _но_ в этом случае не будет возможности определить момент, когда новый конфиг
    // полностью применён.

    fireConfigReload (new_config);
    mutex.unlock ();
}

static StRef<String> doGetAddrStr (ConstMemory      const host,
                                   IpAddress_NoPort const local_addr,
                                   Uint32           const bind_port,
                                   ConstMemory      const preset_addr,
                                   Uint32           const default_port)
{
    if (preset_addr.len())
        return st_grab (new (std::nothrow) String (preset_addr));

    if (host.len()) {
        if (bind_port == default_port)
            return makeString (host);

        return makeString (host, ":", bind_port);
    }

    if (bind_port == default_port)
        return makeString (local_addr);

    return makeString (local_addr, ":", bind_port);
}

StRef<String>
ConfigManager::getRtmpAddrStr (ConstMemory const host_,
                               IpAddress   const local_addr)
{
    ConstMemory const host = extractDomainFromUrl (host_);
    mutex.lock();
    StRef<String> const result = doGetAddrStr (host, local_addr, rtmp_bind_port, String::mem (rtmp_addr_preset), 1935);
    mutex.unlock ();
    return result;
}

StRef<String>
ConfigManager::getRtmptAddrStr (ConstMemory const host_,
                                IpAddress   const local_addr)
{
    ConstMemory const host = extractDomainFromUrl (host_);
    mutex.lock ();
    StRef<String> const result = doGetAddrStr (host, local_addr, rtmpt_bind_port, String::mem (rtmpt_addr_preset), 80);
    mutex.unlock ();
    return result;
}

StRef<String>
ConfigManager::getHlsAddrStr (ConstMemory const host_,
                              IpAddress   const local_addr)
{
    ConstMemory const host = extractDomainFromUrl (host_);
    mutex.lock ();
    StRef<String> const result = doGetAddrStr (host, local_addr, hls_bind_port, String::mem (hls_addr_preset), 80);
    mutex.unlock ();
    return result;
}

StRef<String>
ConfigManager::getRtspAddrStr (ConstMemory const host_,
                               IpAddress   const local_addr)
{
    ConstMemory const host = extractDomainFromUrl (host_);
    mutex.lock ();
    StRef<String> const result = doGetAddrStr (host, local_addr, rtsp_bind_port, String::mem (rtsp_addr_preset), 554);
    mutex.unlock ();
    return result;
}

StRef<String>
ConfigManager::getHttpAddrStr (ConstMemory const host_,
                               IpAddress   const local_addr)
{
    ConstMemory const host = extractDomainFromUrl (host_);
    mutex.lock ();
    StRef<String> const result = doGetAddrStr (host, local_addr, http_bind_port, String::mem (http_addr_preset), 80);
    mutex.unlock ();
    return result;
}

StRef<String>
ConfigManager::getAdminAddrStr (ConstMemory const host_,
                                IpAddress   const local_addr)
{
    ConstMemory const host = extractDomainFromUrl (host_);
    mutex.lock ();
    StRef<String> const result = doGetAddrStr (host, local_addr, admin_bind_port, String::mem (admin_addr_preset), 80);
    mutex.unlock ();
    return result;
}

mt_const void
ConfigManager::init (MConfig::Config * const mt_nonnull config)
{
    this->config = config;
    parseDefaultVarlist (config);
}

ConfigManager::~ConfigManager ()
{
    mutex.lock ();
    releaseDefaultVarHash ();
    mutex.unlock ();
}

}

