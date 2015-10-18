/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <mycpp/io.h>

#include <scruffy/cpp_util.h>

using namespace MyCpp;

namespace Scruffy {

namespace Cpp {

static void
do_dumpContainerInfo (Container const &container,
		      bool      const  dump_parent)
{
    switch (container.getType ()) {
	case Container::t_Namespace: {
	    Namespace const &namespace_ = static_cast <Namespace const &> (container);
	    errf->print ("namespace ").print (namespace_.getPrimaryName ());
	} break;
	case Container::t_Class: {
	    Class const &class_ = static_cast <Class const &> (container);
	    errf->print ("class");
	    if (!class_.name.isNull ())
		errf->print (" ").print (class_.name->toString ());
	} break;
	default:
	    abortIfReached ();
    }

    if (dump_parent) {
	errf->print (" (parent ");
	if (container.getParentContainer () != NULL) {
	    do_dumpContainerInfo (*container.getParentContainer (), false /* dump_parent */);
	}
	errf->print (")");
    }
}

void
dumpContainerInfo (Container const &container)
{
    do_dumpContainerInfo (container, true /* dump_parent */);
    errf->print ("\n");
}

void
dumpMemberInfo (Member const * const member /* non-null */)
{
    switch (member->getType ()) {
	case Member::t_Type: {
	    errf->print ("_Type");
	} break;
	case Member::t_Object: {
	    errf->print ("_Object");
	} break;
	case Member::t_Function: {
	    errf->print ("_Function");
	} break;
	case Member::t_TypeTemplateParameter: {
	    errf->print ("_TypeTemplateParameter");
	} break;
	case Member::t_DependentType: {
	    errf->print ("_DependentType");
	} break;
	case Member::t_DependentObject: {
	    errf->print ("_DependentObject");
	} break;
	default:
	    abortIfReached ();
    }

    if (!member->name.isNull ())
	errf->print (" ").print (member->name->toString ());
}

}

}

