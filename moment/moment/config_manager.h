/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__CONFIG_MANAGER__H__
#define MOMENT__CONFIG_MANAGER__H__


#include <mconfig/mconfig.h>


namespace Moment {

using namespace M;

class ConfigManager : public Object
{
  private:
    StateMutex mutex;


  // _________________________________ Events __________________________________

  public:
    struct Events
    {
        void (*configReload) (MConfig::Config *new_config,
                              void            *cb_data);
    };

  private:
    Informer_<Events> event_informer;

    static void informConfigReload (Events * const events,
                                    void   * const cb_data,
                                    void   * const _new_config)
    {
        if (events->configReload) {
            MConfig::Config * const new_config = static_cast <MConfig::Config*> (_new_config);
            events->configReload (new_config, cb_data);
        }
    }

    void fireConfigReload (MConfig::Config * const new_config)
        { event_informer.informAll (informConfigReload, new_config); }

  public:
    Informer_<Events>* getEventInformer () { return &event_informer; }

  // ___________________________________________________________________________


  public:
    class VarHashEntry : public HashEntry<>
    {
    public:
        MConfig::Varlist::Var *var;
    };

    typedef Hash< VarHashEntry,
                  ConstMemory,
                  MemberExtractor< VarHashEntry,
                                   MConfig::Varlist::Var*,
                                   &VarHashEntry::var,
                                   ConstMemory,
                                   AccessorExtractor< MConfig::Varlist::Var,
                                                      ConstMemory,
                                                      &MConfig::Varlist::Var::getName > >,
                  MemoryComparator<>,
                  DefaultStringHasher >
            VarHash;

  private:
    mt_mutex (mutex) Ref<MConfig::Config>  config;
    mt_mutex (mutex) Ref<MConfig::Varlist> default_varlist;
    mt_mutex (mutex) VarHash               default_var_hash;

    mt_one_of(( mt_const, mt_mutex (mutex) )) void setOverridableVar (ConstMemory    var_name,
                                                                             ConstMemory    opt_name,
                                                                             ConstMemory    host,
                                                                             Uint32         port,
                                                                             StRef<String> * mt_nonnull ret_preset);

    mt_one_of(( mt_const, mt_mutex (mutex) )) void parseDefaultVarlist (MConfig::Config * mt_nonnull new_config);

    mt_mutex (mutex) void releaseDefaultVarHash ();

  public:
    Ref<MConfig::Config>  getConfig ();
    Ref<MConfig::Varlist> getDefaultVarlist ();

    mt_mutex (mutex) MConfig::Config* getConfig_locked         () { return config; }
    mt_mutex (mutex) VarHash*         getDefaultVarHash_locked () { return &default_var_hash; }

    void setConfig (MConfig::Config * mt_nonnull new_config);

    void configManagerLock   () { mutex.lock (); }
    void configManagerUnlock () { mutex.unlock (); }

  private:
    mt_mutex (mutex) StRef<String> rtmp_addr_preset;
    mt_mutex (mutex) StRef<String> rtmpt_addr_preset;
    mt_mutex (mutex) StRef<String> hls_addr_preset;
    mt_mutex (mutex) StRef<String> rtsp_addr_preset;
    mt_mutex (mutex) StRef<String> http_addr_preset;
    mt_mutex (mutex) StRef<String> admin_addr_preset;

    mt_mutex (mutex) Uint32 rtmp_bind_port;
    mt_mutex (mutex) Uint32 rtmpt_bind_port;
    mt_mutex (mutex) Uint32 hls_bind_port;
    mt_mutex (mutex) Uint32 rtsp_bind_port;
    mt_mutex (mutex) Uint32 http_bind_port;
    mt_mutex (mutex) Uint32 admin_bind_port;

  public:
    StRef<String> getRtmpAddrStr  (ConstMemory host, IpAddress local_addr);
    StRef<String> getRtmptAddrStr (ConstMemory host, IpAddress local_addr);
    StRef<String> getHlsAddrStr   (ConstMemory host, IpAddress local_addr);
    StRef<String> getRtspAddrStr  (ConstMemory host, IpAddress local_addr);
    StRef<String> getHttpAddrStr  (ConstMemory hsot, IpAddress local_addr);
    StRef<String> getAdminAddrStr (ConstMemory host, IpAddress local_addr);

  public:
    mt_const void init (MConfig::Config * mt_nonnull config);

    ConfigManager (EmbedContainer * const embed_container)
        : Object         (embed_container),
          event_informer (this /* outer_object */, &mutex)
    {}

    ~ConfigManager ();
};

}


#endif /* MOMENT__CONFIG_MANAGER__H__ */

