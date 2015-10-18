/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MCONFIG__CONFIG_PARSER__H__
#define MCONFIG__CONFIG_PARSER__H__


#include <mconfig/config.h>


namespace MConfig {

using namespace M;

Result parseConfig (ConstMemory  filename,
		    Config      *config);

mt_throws Result parseConfig_Memory_NoPreprocessor (ConstMemory  mem,
                                                    Config      *config);

}


#endif /* MCONFIG__CONFIG_PARSER__H__ */

