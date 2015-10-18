/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef MOMENT__CHANNEL_SET__H__
#define MOMENT__CHANNEL_SET__H__


#include <libmary/libmary.h>
#include <moment/channel.h>


namespace Moment {

using namespace M;

//#warning ChannelSet is deprecated
class ChannelSet
{
private:
    StateMutex mutex;

    class ChannelEntry : public HashEntry<>
    {
    public:
	Ref<Channel> channel;
	StRef<String> channel_name;
    };

    typedef Hash< ChannelEntry,
		  Memory,
		  MemberExtractor< ChannelEntry,
				   StRef<String>,
				   &ChannelEntry::channel_name,
				   Memory,
				   AccessorExtractor< String,
						      Memory,
						      &String::mem > >,
		  MemoryComparator<> >
	    ChannelEntryHash;

    mt_mutex (mutex) ChannelEntryHash channel_entry_hash;

public:
    typedef ChannelEntry* ChannelKey;

    ChannelKey addChannel (Channel     *channel,
			   ConstMemory  channel_name);

    void removeChannel (ChannelKey channel_key);

    Ref<Channel> lookupChannel (ConstMemory channel_name);

    ~ChannelSet ();
};

}


#endif /* MOMENT__CHANNEL_SET__H__ */

