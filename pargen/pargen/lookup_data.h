/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef PARGEN__LOOKUP_DATA__H__
#define PARGEN__LOOKUP_DATA__H__


#include <libmary/libmary.h>


namespace Pargen {

using namespace M;

class LookupData : public StReferenced
{
public:
    virtual void newCheckpoint () = 0;

    virtual void commitCheckpoint () = 0;

    virtual void cancelCheckpoint () = 0;
};

}


#endif /* PARGEN__LOOKUP_DATA__H__ */

