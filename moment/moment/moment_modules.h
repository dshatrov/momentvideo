#ifndef MOMENT__MOMENT_MODULES__H__
#define MOMENT__MOMENT_MODULES__H__


#include <moment/moment_instance.h>


namespace Moment {

Result loadModules (MomentInstance * mt_nonnull moment_instance,
                    ConstMemory     module_path);

}


#endif /* MOMENT__MOMENT_MODULES__H__ */

