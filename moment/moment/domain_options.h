#ifndef MOMENT__DOMAIN_OPTIONS__H__
#define MOMENT__DOMAIN_OPTIONS__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

typedef List< StRef<String> > DomainList;

class DomainOptions : public Referenced
{
  public:
    DomainList allowed_domains;
};

}


#endif /* MOMENT__DOMAIN_OPTIONS__H__ */

