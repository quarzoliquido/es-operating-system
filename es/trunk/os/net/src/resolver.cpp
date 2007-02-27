/*
 * Copyright (c) 2006, 2007
 * Nintendo Co., Ltd.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Nintendo makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

#include "resolver.h"

int Resolver::
Control::a(u16 id, const char* hostName)
{
    DNSHdr* dns = reinterpret_cast<DNSHdr*>(query);
    dns->id = htons(id);
    dns->flags = htons(DNSHdr::Query | DNSHdr::StandardQuery | DNSHdr::RD);
    dns->qdcount = htons(1);
    dns->ancount = htons(0);
    dns->nscount = htons(0);
    dns->arcount = htons(0);

    // Put name into DNS name format
    bool dot = false;
    u8* opt = query + sizeof(DNSHdr);
    u8* org = opt;
    while (*hostName)
    {
        u8* count = opt++;
        u8 i;
        for (i = 0; *hostName && *hostName != '.'; ++i)
        {
            if (DNSHdr::LabelMax <= i || DNSHdr::NameMax - 1 <= opt - org)
            {
                return 0;
            }
            *opt++ = (u8) *hostName++;
        }
        if (i == 0)
        {
            return 0;
        }
        *count = i;
        if (*hostName == '.')
        {
            dot = true;
            ++hostName;
        }
        else if (!dot && suffix[0])
        {
            dot = true;
            hostName = suffix;
        }
    }
    if (opt == org)
    {
        return 0;
    }
    *opt++ = 0;

    *(u16*) opt = htons(DNSType::A);
    opt += 2;

    *(u16*) opt = htons(DNSClass::IN);
    opt += 2;

    return opt - query;
}

int Resolver::
Control::ptr(u16 id, InAddr addr)
{
    DNSHdr* dns = reinterpret_cast<DNSHdr*>(query);
    dns->id = htons(id);
    dns->flags = htons(DNSHdr::Query | DNSHdr::StandardQuery | DNSHdr::RD);
    dns->qdcount = htons(1);
    dns->ancount = htons(0);
    dns->nscount = htons(0);
    dns->arcount = htons(0);

    // Put name into DNS name format
    u8* opt = query + sizeof(DNSHdr);
    u8* org = opt;
    u32 h = ntohl(addr.addr);
    for (int i = 0; i < 4; ++i)
    {
        u8* count = opt++;
        *count = (u8) sprintf((char*) opt, "%u", (u8) (h >> (8 * i)));
        opt += *count;
    }
    *opt++ = 7;
    memmove(opt, "in-addr", 7);
    opt += 7;
    *opt++ = 4;
    memmove(opt, "arpa", 4);
    opt += 4;
    *opt++ = 0;

    *(u16*) opt = htons(DNSType::PTR);
    opt += 2;

    *(u16*) opt = htons(DNSClass::IN);
    opt += 2;

    return opt - query;
}

u8* Resolver::
Control::skipName(const u8* ptr, const u8* end)
{
    while (ptr < end)
    {
        if (*ptr == 0)
        {
            ++ptr;
            return (ptr <= end) ? (u8*) ptr : (u8*) end;
        }
        u8 count = *ptr;
        if (count & 0xc0)
        {
            if ((count & 0xc0) != 0xc0)
            {
                return 0;
            }
            ptr += 2;
            return (ptr <= end) ? (u8*) ptr : (u8*) end;
        }
        ptr += 1 + count;
    }
    return (u8*) end;
}

bool Resolver::
Control::copyName(const u8* dns, const u8* ptr, const u8* end, char* name)
{
    int namelen = DNSHdr::NameMax;
    while (dns <= ptr && ptr < end)
    {
        u8 count = *ptr;
        if (count & 0xc0)
        {
            if ((count & 0xc0) != 0xc0)
            {
                return false;
            }
            ptr = dns + (ntohs(*(u16*) ptr) & ~0xc000);
            continue;
        }
        if (end <= ptr + count)
        {
            return false;
        }
        if (namelen < count + 1)
        {
            return false;
        }
        namelen -= count + 1;

        memmove(name, ++ptr, (size_t) count);
        ptr += count;
        name += count;
        if (*ptr != 0)
        {
            *name++ = '.';
        }
        else
        {
            *name = '\0';
            return true;
        }
    }
    return false;
}

Resolver::
Control::Control(IInternetAddress* server) :
    server(server)
{
    memset(suffix, 0, sizeof suffix);

    Handle<IInternetAddress> any = Socket::resolver->getHostByAddress(&InAddrAny.addr, sizeof(InAddr), server->getScopeID());
    socket = any->socket(AF_INET, ISocket::Datagram, 0);
    socket->connect(server, DNSHdr::Port);
}

Resolver::
Control::~Control()
{
    socket->close();
}

IInternetAddress* Resolver::
Control::getHostByName(const char* hostName, int addressFamily)
{
    if (!hostName)
    {
        return 0;
    }

    if (addressFamily != AF_INET)
    {
        return 0;
    }

    u16 xid = id.increment();
    int len = a(xid, hostName);
    if (len <= 0)
    {
        return 0;
    }

    for (int rxmitCount = 0; rxmitCount < MaxQuery; ++rxmitCount)
    {
        socket->write(query, len);
        socket->setTimeout(TimeSpan(0, 0, MinWait << rxmitCount));
        int rlen = socket->read(response, sizeof response);
        if (rlen <= sizeof(DNSHdr))
        {
            continue;
        }
        DNSHdr* dns = reinterpret_cast<DNSHdr*>(response);
        if (dns->getID() != xid ||
            !dns->isResponse() ||
            ntohs(dns->qdcount) != 1 ||
            ntohs(dns->ancount) == 0)
        {
            continue;
        }
        u8* opt = response + sizeof(DNSHdr);
        u8* end = response + rlen;
        opt = skipName(opt, end);
        if (end < opt + 4)
        {
            continue;
        }
        opt += 4;
        if (len < opt - response ||
            memcmp(query + sizeof(DNSHdr),
                   response + sizeof(DNSHdr),
                   opt - (response + sizeof(DNSHdr))) != 0)
        {
            continue;
        }

        for (int i = 0; i < ntohs(dns->ancount); ++i)
        {
            opt = skipName(opt, end);
            if (end - opt < sizeof(DNSRR))
            {
                continue;
            }
            DNSRR* rr = reinterpret_cast<DNSRR*>(opt);
            if (ntohs(rr->type) != DNSType::A ||
                ntohs(rr->cls) != DNSClass::IN ||
                ntohs(rr->rdlength) != sizeof(InAddr))
            {
                continue;
            }
            if (end < opt + DNSRR::Size + ntohs(rr->rdlength))
            {
                break;
            }
            InAddr addr = *reinterpret_cast<InAddr*>(opt + DNSRR::Size);
            IInternetAddress* host = Socket::resolver->getHostByAddress(&addr.addr, sizeof(InAddr), 0);
            return host;
        }
    }

    return 0;
}

bool Resolver::
Control::getHostName(IInternetAddress* address, char* hostName, unsigned int nlen)
{
    InAddr addr;

    if (nlen < DNSHdr::NameMax)
    {
        return false;
    }

    if (address->getAddress(&addr, sizeof(InAddr)) != sizeof(InAddr))
    {
        return false;
    }

    u16 xid = id.increment();
    int len = ptr(xid, addr);
    if (len <= 0)
    {
        return false;
    }

    for (int rxmitCount = 0; rxmitCount < MaxQuery; ++rxmitCount)
    {
        socket->write(query, len);
        socket->setTimeout(TimeSpan(0, 0, MinWait << rxmitCount));
        int rlen = socket->read(response, sizeof response);
        if (rlen <= sizeof(DNSHdr))
        {
            continue;
        }
        DNSHdr* dns = reinterpret_cast<DNSHdr*>(response);
        if (dns->getID() != xid ||
            !dns->isResponse() ||
            ntohs(dns->qdcount) != 1 ||
            ntohs(dns->ancount) == 0)
        {
            continue;
        }
        u8* opt = response + sizeof(DNSHdr);
        u8* end = response + rlen;
        opt = skipName(opt, end);
        if (end < opt + 4)
        {
            continue;
        }
        opt += 4;
        if (len < opt - response ||
            memcmp(query + sizeof(DNSHdr),
                   response + sizeof(DNSHdr),
                   opt - (response + sizeof(DNSHdr))) != 0)
        {
            continue;
        }

        for (int i = 0; i < ntohs(dns->ancount); ++i)
        {
            opt = skipName(opt, end);
            if (end - opt < sizeof(DNSRR))
            {
                continue;
            }
            DNSRR* rr = reinterpret_cast<DNSRR*>(opt);
            if (ntohs(rr->type) != DNSType::PTR ||
                ntohs(rr->cls) != DNSClass::IN)
            {
                continue;
            }

            if (end < opt + DNSRR::Size + ntohs(rr->rdlength))
            {
                break;
            }

            if (!copyName(response, opt + DNSRR::Size, end, hostName))
            {
                break;
            }
            return true;
        }
    }

    return false;
}

bool Resolver::
setup()
{
    Handle<IInternetAddress> nameServer = Socket::config->getNameServer();
    if (!nameServer)
    {
        if (control)
        {
            delete control;
            control = 0;
        }
        return false;
    }

    if (control)
    {
        return true;
    }

    control = new Control(nameServer);
    return true;
}

IInternetAddress* Resolver::
getHostByName(const char* hostName, int addressFamily)
{
    Synchronized<IMonitor*> method(monitor);

    if (!setup())
    {
        return 0;
    }

    return control->getHostByName(hostName, addressFamily);
}

bool Resolver::
getHostName(IInternetAddress* address, char* hostName, unsigned int len)
{
    Synchronized<IMonitor*> method(monitor);

    if (!setup())
    {
        return false;
    }

    return control->getHostName(address, hostName, len);
}

// Note DNS is not queried.
IInternetAddress* Resolver::
getHostByAddress(const void* address, unsigned int len, unsigned int scopeID)
{
    if (len == sizeof(InAddr))  // AF_INET
    {
        InAddr addr;
        memmove(&addr.addr, address, sizeof(InAddr));

        InFamily* inFamily = dynamic_cast<InFamily*>(Socket::getAddressFamily(AF_INET));
        Inet4Address* host = inFamily->getAddress(addr, scopeID);
        if (!host)
        {
            Handle<Inet4Address> onLink;
            if (IN_IS_ADDR_LOOPBACK(addr))
            {
                host = new Inet4Address(addr, Inet4Address::statePreferred, 1, 8);
            }
            else if (IN_IS_ADDR_MULTICAST(addr))
            {
                host = new Inet4Address(addr, Inet4Address::stateNonMember, scopeID);
            }
            else if (onLink = inFamily->onLink(addr, scopeID))
            {
                host = new Inet4Address(addr, Inet4Address::stateInit, onLink->getScopeID());
            }
            else
            {
                host = new Inet4Address(addr, Inet4Address::stateDestination, scopeID);
            }
            inFamily->addAddress(host);
        }
        return host;
    }

    if (len == sizeof(In6Addr)) // AF_INET6
    {
        return 0;
    }

    return 0;
}

bool Resolver::
queryInterface(const Guid& riid, void** objectPtr)
{
    if (riid == IID_IResolver)
    {
        *objectPtr = static_cast<IResolver*>(this);
    }
    else if (riid == IID_IInterface)
    {
        *objectPtr = static_cast<IResolver*>(this);
    }
    else
    {
        *objectPtr = NULL;
        return false;
    }
    static_cast<IInterface*>(*objectPtr)->addRef();
    return true;
}

unsigned int Resolver::
addRef()
{
    return ref.addRef();
}

unsigned int Resolver::
release()
{
    unsigned int count = ref.release();
    if (count == 0)
    {
        delete this;
        return 0;
    }
    return count;
}

Resolver::
Resolver() :
    control(0)
{
    esCreateInstance(CLSID_Monitor,
                     IID_IMonitor,
                     reinterpret_cast<void**>(&monitor));
}

Resolver::
~Resolver()
{
    if (control)
    {
        delete control;
        control = 0;
    }
}
