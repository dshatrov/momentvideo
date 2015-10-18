/*  Copyright (C) 2014 Dmitry Shatrov - All Rights Reserved
    e-mail: info@momentvideo.org

    Unauthorized copying of this file or any part of its contents, 
    via any medium is strictly prohibited.

    Proprietary and confidential.
 */


#ifndef LIBMARY__ADDRESS_SAVER__H__
#define LIBMARY__ADDRESS_SAVER__H__


#include <libmary/intrusive_list.h>
#include <libmary/mutex.h>
#include <libmary/util_net.h>


namespace M {

mt_unsafe class AddressSaver
{
private:
    Mutex mutex;

public:
    class PortEntry : public IntrusiveListElement<>
    {
    public:
        Uint16 port;
    };
    typedef IntrusiveList< PortEntry, IntrusiveList_name, DeleteAction<PortEntry> > PortList;

    class AddressEntry : public IntrusiveListElement<>
    {
    public:
        Uint32   ip_addr;
        unsigned num_ports;
        PortList port_list;
    };
    typedef IntrusiveList< AddressEntry, IntrusiveList_name, DeleteAction<AddressEntry> > AddressList;

private:
    mt_const unsigned max_addrs;
    mt_const unsigned max_ports;

    mt_mutex (mutex) unsigned num_addrs;
    mt_mutex (mutex) AddressList addr_list;

    mt_mutex (mutex) void addPort (AddressEntry * const addr_entry,
                                   Uint16         const port)
    {
        if (max_ports == 0)
            return;

        PortList::iterator port_iter (addr_entry->port_list);
        while (!port_iter.done()) {
            PortEntry * const port_entry = port_iter.next ();
            if (port_entry->port == port) {
                addr_entry->port_list.removeNoAction (port_entry);
                addr_entry->port_list.prepend (port_entry);
                return;
            }
        }

        PortEntry * const port_entry = new (std::nothrow) PortEntry;
        assert (port_entry);
        port_entry->port = port;
        addr_entry->port_list.prepend (port_entry);

        if (addr_entry->num_ports >= max_ports)
            addr_entry->port_list.remove (addr_entry->port_list.getLast());
        else
            ++addr_entry->num_ports;
    }

public:
    unsigned     getNumAddrs     () { return num_addrs; }
    AddressList* getAddressList  () { return &addr_list; }

    void lock   () { mutex.lock (); }
    void unlock () { mutex.unlock (); }

    void addAddress (IpAddress const addr)
    {
        if (max_addrs == 0)
            return;

        mutex.lock ();

        AddressList::iterator addr_iter (addr_list);
        while (!addr_iter.done()) {
            AddressEntry * const addr_entry = addr_iter.next ();
            if (addr_entry->ip_addr == addr.ip_addr) {
                addr_list.removeNoAction (addr_entry);
                addr_list.prepend (addr_entry);
                addPort (addr_entry, addr.port);

                mutex.unlock ();
                return;
            }
        }

        AddressEntry * const addr_entry = new (std::nothrow) AddressEntry;
        assert (addr_entry);
        addr_entry->ip_addr = addr.ip_addr;
        addr_entry->num_ports = 0;
        addr_list.prepend (addr_entry);

        if (num_addrs >= max_addrs)
            addr_list.remove (addr_list.getLast());
        else
            ++num_addrs;

        addPort (addr_entry, addr.port);

        mutex.unlock ();
    }

    mt_const void init (unsigned const max_addrs,
                        unsigned const max_ports)
    {
        this->max_addrs = max_addrs;
        this->max_ports = max_ports;
    }

    AddressSaver ()
        : max_addrs (0),
          max_ports (0),
          num_addrs (0)
    {}

    ~AddressSaver ()
    {
        mutex.lock ();
        addr_list.clear ();
        mutex.unlock ();
    }
};

}


#endif /* LIBMARY__ADDRESS_SAVER__H__ */

