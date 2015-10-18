/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__VIRT_REFRENCED__H__
#define LIBMARY__VIRT_REFRENCED__H__


namespace M {

class VirtReferenced
{
public:
    virtual void virt_ref   () = 0;
    virtual void virt_unref () = 0;

    virtual ~VirtReferenced () {}
};

}


#endif /* LIBMARY__VIRT_REFRENCED__H__ */

