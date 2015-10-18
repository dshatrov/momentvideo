/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__TYPE__H__
#define SCRUFFY__TYPE__H__

#include <mycpp/mycpp.h>

namespace Scruffy {

using namespace MyCpp;

#if 0
class SimpleDeclaration
{
public:
    TypeDesc type_desc;

    List<Pointer> pointers;

    Bool global_namespace;
    List<NestedNamePart> nested_name;
};

class Variable : public SimpleDeclaration
{
public:
    StRef<String> identifier;
};

class OperatorFunction : public SimpleDeclaration
{
public:
};

class ConversionFunction : public SimpleDeclaration
{
public:
};

class Destructor : public SimpleDeclaration
{
public:
};

class TemplateDeclaration : public SimpleDeclaration
{
public:
};
#endif

}

#endif /* SCRUFFY__TYPE__H__ */

