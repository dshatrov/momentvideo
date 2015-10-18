/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <libmary/module_init.h>
#include <moment/moment_instance.h>

#include <moment-nvr/moment_nvr_module.h>


namespace MomentNvr {
    Result momentNvrInit (MomentInstance * const mt_nonnull moment_instance)
        { return commonMomentModuleInit <MomentNvrModule> (moment_instance, "mod_nvr", /*default_enable=*/ false, _func); }
}

#ifndef MOMENT_STATIC_MODULES
  extern "C" bool
  libMary_moduleInit (void * const moment_instance_)
          { return MomentNvr::momentNvrInit (static_cast <Moment::MomentInstance*> (moment_instance_)); }
#endif

