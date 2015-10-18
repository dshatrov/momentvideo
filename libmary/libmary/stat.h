/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__STAT__H___
#define LIBMARY__STAT__H___


#include <libmary/types.h>
#include <libmary/referenced.h>
#include <libmary/list.h>
#include <libmary/hash.h>
#include <libmary/string.h>


namespace M {

class Stat
{
private:
    Mutex mutex;

public:
    enum ParamType
    {
        ParamType_Int64,
        ParamType_Double
    };

    class ParamKey
    {
    private:
        void *key;
    public:
        operator void* () const { return key; }
        ParamKey (void * const key) : key (key) {}
        ParamKey () : key (NULL) {}
    };

    class StatSlave : public virtual Referenced
    {
    public:
        virtual ParamKey createParam (ConstMemory param_name,
                                      ConstMemory param_desc,
                                      ParamType   param_type,
                                      Int64       int64_value,
                                      double      double_value) = 0;

        virtual void setInt (ParamKey param,
                             Int64    value) = 0;

        virtual void setDouble (ParamKey param,
                                double   value) = 0;
    };

    // TODO class InternalStatParam
    class StatParam : public HashEntry<>
    {
    public:
        mt_const StRef<String> param_name;
        mt_const StRef<String> param_desc;
        mt_const ParamType   param_type;
        mt_mutex (mutex) Int64  int64_value;
        mt_mutex (mutex) double double_value;

        ParamKey slave_param;
    };

    typedef Hash< StatParam,
                  ConstMemory,
                  MemberExtractor< StatParam,
                                   StRef<String>,
                                   &StatParam::param_name,
                                   ConstMemory,
                                   AccessorExtractor< String,
                                                      Memory,
                                                      &String::mem > >,
                  MemoryComparator<> >
            StatParamHash;

private:
    mt_mutex (mutex) StatParamHash stat_param_hash;

    mt_mutex (mutex) Ref<StatSlave> stat_slave;

public:
    mt_locks (mutex) void lock ()
    {
        mutex.lock ();
    }

    mt_unlocks (mutex) void unlock ()
    {
        mutex.unlock ();
    }

    ParamKey getParam (ConstMemory param_name);

    ParamKey getParam_locked (ConstMemory param_name);

    void getAllParams (List<StatParam> * mt_nonnull ret_params);

    StatParamHash* getStatParamHash_locked ()
    {
        return &stat_param_hash;
    }

    ParamKey createParam (ConstMemory param_name,
                          ConstMemory param_desc,
                          ParamType   param_type,
                          Int64       int64_value,
                          double      double_value);

    void setInt (ParamKey param,
                 Int64    value);

    void setDouble (ParamKey param,
                    double   value);

    void addInt (ParamKey param,
                 Int64    delta);

    void addDouble (ParamKey param,
                    double   delta);

    Int64 getInt (ParamKey param);

    double getDouble (ParamKey param);

    Int64 getInt_locked (ParamKey param);

    double getDouble_locked (ParamKey param);

    void inc (ParamKey param)
    {
        addInt (param, 1);
    }

    void dec (ParamKey param)
    {
        addInt (param, -1);
    }

    void setStatSlave (StatSlave * const stat_slave)
    {
        mutex.lock ();
        this->stat_slave = stat_slave;
        mutex.unlock ();
    }

    // FIXME Racy, since we access 'stat_slave' with 'mutex' unlocked.
    void releaseStatSlave ()
    {
        setStatSlave (NULL);
    }

    ~Stat ();
};

extern Stat *_libMary_stat;

static inline Stat* getStat ()
{
    return _libMary_stat;
}

}


#endif /* LIBMARY__STAT__H___ */

