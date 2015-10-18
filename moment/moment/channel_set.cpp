/*  Copyright (C) 2011-2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#include <moment/channel_set.h>


using namespace M;

namespace Moment {

ChannelSet::ChannelKey
ChannelSet::addChannel (Channel     * const channel,
			ConstMemory   const channel_name)
{
    ChannelEntry * const channel_entry = new (std::nothrow) ChannelEntry;
    assert (channel_entry);
    channel_entry->channel = channel;
    channel_entry->channel_name = st_grab (new (std::nothrow) String (channel_name));

    mutex.lock ();
    channel_entry_hash.add (channel_entry);
    mutex.unlock ();

    return channel_entry;
}

void
ChannelSet::removeChannel (ChannelKey const channel_key)
{
    mutex.lock ();
    channel_entry_hash.remove (channel_key);
    mutex.unlock ();
}

Ref<Channel>
ChannelSet::lookupChannel (ConstMemory const channel_name)
{
    mutex.lock ();

    ChannelEntry * const channel_entry = channel_entry_hash.lookup (channel_name);
    if (!channel_entry) {
	mutex.unlock ();
	return NULL;
    }

    Ref<Channel> const channel = channel_entry->channel;

    mutex.unlock ();

    return channel;
}

ChannelSet::~ChannelSet ()
{
    mutex.lock ();

    ChannelEntryHash::iter iter (channel_entry_hash);
    while (!channel_entry_hash.iter_done (iter)) {
	ChannelEntry * const channel_entry = channel_entry_hash.iter_next (iter);
	delete channel_entry;
    }

    mutex.unlock ();
}

}

