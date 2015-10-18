#ifndef MOMENT__UTIL_H264__H__
#define MOMENT__UTIL_H264__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

void dumpH264AvcNalUnits (PagePool::Page *page,
                          size_t          msg_offs,
                          size_t          msg_len);

}


#endif /* MOMENT__UTIL_H264__H__ */

