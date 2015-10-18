/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__CPP_PARSER_H__
#define SCRUFFY__CPP_PARSER_H__

#include <pargen/lookup_data.h>

#include <scruffy/cpp.h>

namespace Scruffy {

using namespace MyCpp;

class CppParser_Impl;

class CppParser : public virtual SimplyReferenced
{
private:
    Ref<CppParser_Impl> impl;

public:
    Ref<Pargen::LookupData> getLookupData ();

    Ref<Cpp::Namespace> getRootNamespace ();

    CppParser (ConstMemoryDesc const &default_variant = ConstMemoryDesc::forString ("default"));

    // This prevents clang from trying to instantiate Ref<CppParser_Impl>::~Ref too early.
    ~CppParser ();
};

}

#endif /* SCRUFFY__CPP_PARSER_H__ */

