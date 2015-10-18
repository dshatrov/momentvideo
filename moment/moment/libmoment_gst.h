/*  Copyright (C) 2012-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__LIBMOMENT_GST__H__
#define MOMENT__LIBMOMENT_GST__H__


#include <libmary/types.h>
#include <gst/gst.h>

#include <libmary/libmary.h>


namespace Moment {

using namespace M;

void dumpGstBufferFlags (GstBuffer *buffer);

void libMomentGstInit (ConstMemory gst_debug_str = ConstMemory());

}


#endif /* MOMENT__LIBMOMENT_GST__H__ */

