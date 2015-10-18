/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef SCRUFFY__NAME_TRACKER_H__
#define SCRUFFY__NAME_TRACKER_H__

#include <mycpp/mycpp.h>

#include <pargen/lookup_data.h>

#include <scruffy/cpp.h>
#include <scruffy/checkpoint_tracker.h>
#include <scruffy/parsing_exception.h>

namespace Scruffy {

using namespace MyCpp;

class NameTrackerException : public InternalException,
			     public ExceptionBase <NameTrackerException>
{
public:
    NameTrackerException (String    *message = String::nullString (),
			  Exception *cause = NULL)
	: InternalException (message, cause)
    {
    }
};

class NameTracker : public virtual SimplyReferenced
{
public:
    class NamespaceKey : public virtual SimplyReferenced
    {
    public:
    };

//private:
public:
    CheckpointTracker checkpoint_tracker;

    List< Ref<Cpp::Namespace> > temporal_namespaces;

    Ref<Cpp::NamespaceEntry> root_namespace_entry;

    Cpp::NamespaceEntry *cur_namespace_entry;

public:
    void newCheckpoint ();

    void commitCheckpoint ();

    void cancelCheckpoint ();


    void beginNamespace (ConstMemoryDesc const &namespace_name);

    void endNamespace ();

    void beginTemporalNamespace ();

    void endTemporalNamespace ();

#if 0
// TODO nested name
    void usingNamespace (ConstMemoryDesc const &namespace_name);
#endif

    Ref<NamespaceKey> lookupNamespace (/* TODO */);

    void addName (Cpp::Name const *name);

    void addObject (Cpp::Object *object,
		    Cpp::Name const *name);

    void addType (/*NamespaceKey const *namespace_key,*/
		  Cpp::TypeDesc *type_desc,
		  String *identifier_str);

    Ref<Cpp::Namespace::TypeEntry> lookupType (/* NamespaceKey const *namespace_key, */
					       ConstMemoryDesc const &identifier_str);

//    void addObject ();

#if 0
    void addName (Cpp::Identifier const *id)
	   throw (NameTrackerException);
#endif

    Ref<Cpp::Name> lookupName (Cpp::Name const *name);

    NameTracker ();
};

}

#endif /* SCRUFFY__NAME_TRACKER_H__ */

