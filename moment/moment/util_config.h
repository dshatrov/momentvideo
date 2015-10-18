/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__UTIL_CONFIG__H__
#define MOMENT__UTIL_CONFIG__H__


#include <libmary/libmary.h>
#include <mconfig/mconfig.h>

#include <moment/channel_options.h>
#include <moment/rate_limit.h>


namespace Moment {

Result parseOverlayConfig (MConfig::Section * mt_nonnull channel_section,
                           ChannelOptions   * mt_nonnull opts);

Result parseAllowedDomains (MConfig::Section *section,
                            ChannelOptions   *default_opts,
                            DomainList       * mt_nonnull domain_list);

Result parseChannelItemConfig (MConfig::Section * mt_nonnull section,
                               PlaybackItem     * mt_nonnull default_item,
                               PlaybackItem     * mt_nonnull item);

Result parseChannelConfig (MConfig::Section * mt_nonnull section,
                           ConstMemory       config_item_name,
                           bool              is_default_channel,
                           ChannelOptions   * mt_nonnull default_opts,
                           OutputDesc       * mt_nonnull default_output_desc,
                           ChannelOptions   * mt_nonnull opts,
                           PlaybackItem     * mt_nonnull item);

Result parseRatelimitConfig (MConfig::Section * mt_nonnull section,
                             RateLimitParams  * mt_nonnull params);


// ______________________ Helper config access functions _______________________

template <class ...Args>
Result configGetInt64 (MConfig::Config * const mt_nonnull config,
                       ConstMemory       const opt_name,
                       Int64           * const mt_nonnull ret_val,
                       Int64             const default_val,
                       Args const      &...args)
{
    *ret_val = default_val;

    if (!config->getInt64_default (opt_name, ret_val, default_val)) {
        logE_ (args..., "invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        return Result::Failure;
    }

    logI_ (args..., opt_name, ": ", *ret_val);
    return Result::Success;
}

template <class ...Args>
Result configGetInt64_ (MConfig::Config * const mt_nonnull config,
                        ConstMemory       const opt_name,
                        Int64           * const mt_nonnull ret_val,
                        Args const      &...args)
{
    return configGetInt64 (config, opt_name, ret_val, *ret_val, args...);
}

template <class ...Args>
Result configGetUint64 (MConfig::Config * const mt_nonnull config,
                        ConstMemory       const opt_name,
                        Uint64          * const mt_nonnull ret_val,
                        Uint64            const default_val,
                        Args const      &...args)
{
    *ret_val = default_val;

    if (!config->getUint64_default (opt_name, ret_val, default_val)) {
        logE_ (args..., "invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        return Result::Failure;
    }

    logI_ (args..., opt_name, ": ", *ret_val);
    return Result::Success;
}

template <class ...Args>
Result configGetUint64_ (MConfig::Config * const mt_nonnull config,
                         ConstMemory       const opt_name,
                         Uint64          * const mt_nonnull ret_val,
                         Args const      &...args)
{
    return configGetUint64 (config, opt_name, ret_val, *ret_val, args...);
}

template <class ...Args>
Result configGetBoolean (MConfig::Config * const mt_nonnull config,
                         ConstMemory       const opt_name,
                         bool            * const mt_nonnull ret_val,
                         bool              const default_val,
                         Args const       &...args)
{
    *ret_val = default_val;

    MConfig::BooleanValue const val = config->getBoolean (opt_name);
    if (val == MConfig::Boolean_Invalid) {
        logE_ (args..., "invalid value for config option ", opt_name, ": ", config->getString (opt_name));
        return Result::Failure;
    } else
    if (val == MConfig::Boolean_True)
        *ret_val = true;
    else
    if (val == MConfig::Boolean_False)
        *ret_val = false;
    else
        assert (val == MConfig::Boolean_Default);

    logI_ (args..., opt_name, ": ", *ret_val);
    return Result::Success;
}

template <class ...Args>
Result configGetBoolean_ (MConfig::Config * const mt_nonnull config,
                          ConstMemory       const opt_name,
                          bool            * const mt_nonnull ret_val,
                          Args const      &...args)
{
    return configGetBoolean (config, opt_name, ret_val, *ret_val, args...);
}

template <class ...Args>
Result configSectionGetInt64 (MConfig::Section * const mt_nonnull section,
                              ConstMemory        const opt_name,
                              Int64            * const mt_nonnull ret_val,
                              Int64              const default_val,
                              Args const       &...args)
{
    *ret_val = default_val;

    MConfig::Option * const opt = section->getOption (opt_name, false /* create */);
    if (!opt)
        return Result::Success;

    MConfig::Value * const val = opt->getValue();
    if (!val)
        return Result::Success;

    if (val->mem().len() == 0)
        return Result::Success;

    Int64 res_val = 0;
    if (!strToInt64_safe (val->mem(), &res_val)) {
        logE_ (args..., "invalid falue for section option ", opt_name, ": ", val->mem());
        return Result::Failure;
    }

    *ret_val = res_val;
    return Result::Success;
}

template <class ...Args>
Result configSectionGetInt64_ (MConfig::Section * const mt_nonnull section,
                               ConstMemory        const opt_name,
                               Int64            * const mt_nonnull ret_val,
                               Args const       &...args)
{
    return configSectionGetInt64 (section, opt_name, ret_val, *ret_val, args...);
}

template <class ...Args>
Result configSectionGetUint64 (MConfig::Section * const mt_nonnull section,
                               ConstMemory        const opt_name,
                               Uint64           * const mt_nonnull ret_val,
                               Uint64             const default_val,
                               Args const       &...args)
{
    *ret_val = default_val;

    MConfig::Option * const opt = section->getOption (opt_name, false /* create */);
    if (!opt)
        return Result::Success;

    MConfig::Value * const val = opt->getValue();
    if (!val)
        return Result::Success;

    if (val->mem().len() == 0)
        return Result::Success;

    Uint64 res_val = 0;
    if (!strToUint64_safe (val->mem(), &res_val)) {
        logE_ (args..., "invalid falue for section option ", opt_name, ": ", val->mem());
        return Result::Failure;
    }

    *ret_val = res_val;
    return Result::Success;
}

template <class ...Args>
Result configSectionGetUint64_ (MConfig::Section * const mt_nonnull section,
                                ConstMemory        const opt_name,
                                Uint64           * const mt_nonnull ret_val,
                                Args const       &...args)
{
    return configSectionGetUint64 (section, opt_name, ret_val, *ret_val, args...);
}

template <class ...Args>
Result configSectionGetBoolean (MConfig::Section * const mt_nonnull section,
                                ConstMemory        const opt_name,
                                bool             * const mt_nonnull ret_val,
                                bool               const default_val,
                                Args const       &...args)
{
    *ret_val = default_val;

    MConfig::Option * const opt = section->getOption (opt_name);
    if (!opt)
        return Result::Success;

    MConfig::BooleanValue const val = opt->getBoolean();
    if (val == MConfig::Boolean_Invalid) {
        MConfig::Value * const mval = opt->getValue();
        logE_ (args..., "invalid value for section option ", opt_name, ": ",
               (mval ? mval->getAsString()->mem() : ConstMemory()));
        return Result::Failure;
    } else
    if (val == MConfig::Boolean_True)
        *ret_val = true;
    else
    if (val == MConfig::Boolean_False)
        *ret_val = false;
    else
        assert (val == MConfig::Boolean_Default);

    return Result::Success;
}

template <class ...Args>
Result configSectionGetBoolean_ (MConfig::Section * const mt_nonnull section,
                                 ConstMemory        const opt_name,
                                 bool             * const mt_nonnull ret_val,
                                 Args const       &...args)
{
    return configSectionGetBoolean (section, opt_name, ret_val, *ret_val, args...);
}

void configWarnNoEffect (ConstMemory opt_name);

// _____________________________________________________________________________


}


#endif /* MOMENT__UTIL_CONFIG__H__ */

