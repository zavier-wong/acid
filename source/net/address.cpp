//
// Created by zavier on 2021/12/6.
//

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sstream>

#include "acid/log.h"
#include "acid/net/address.h"

namespace acid{
static acid::Logger::ptr g_logger = ACID_LOG_NAME("system");

Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen){
    if(addr == nullptr) {
        return nullptr;
    }

    Address::ptr result;
    switch(addr->sa_family) {
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        default:
            result.reset(new UnknownAddress(*addr));
            break;
    }
    return result;
}

Address::ptr Address::LookupAny(const std::string& host,int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)) {
        return result[0];
    }
    return nullptr;
}

IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host,int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)) {
        for(auto& i : result) {
            IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
            if(v) {
                return v;
            }
        }
    }
    return nullptr;
}


bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                     int family, int type, int protocol) {
    addrinfo hints, *results, *next;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    std::string node;
    const char* service = NULL;

    //检查 ipv6address serivce
    if(!host.empty() && host[0] == '[') {
        const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
        if(endipv6) {
            //TODO check out of range
            if(*(endipv6 + 1) == ':') {
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    //检查 node serivce
    if(node.empty()) {
        service = (const char*)memchr(host.c_str(), ':', host.size());
        if(service) {
            if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    if(node.empty()) {
        node = host;
    }
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if(error) {
        ACID_LOG_DEBUG(g_logger) << "Address::Lookup getaddress(" << host << ", "
                                  << family << ", " << type << ") err=" << error << " errstr="
                                  << gai_strerror(error);
        return false;
    }

    next = results;
    while(next) {
        result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }

    freeaddrinfo(results);
    return !result.empty();
}

bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,int family) {
    struct ifaddrs *next, *results;
    int error = getifaddrs(&results);
    if(error) {
        ACID_LOG_DEBUG(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
                                     " err=" << error << " errstr=" << gai_strerror(error);
        return false;
    }

    try {
        for(next = results; next; next = next->ifa_next) {
            Address::ptr addr;
            uint32_t prefix_len = ~0u;
            if(family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                continue;
            }
            switch(next->ifa_addr->sa_family) {
                case AF_INET:
                {
                    addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                    uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                    prefix_len = CountBytes(netmask);
                }
                    break;
                case AF_INET6:
                {
                    addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                    in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                    prefix_len = 0;
                    for(int i = 0; i < 16; ++i) {
                        prefix_len += CountBytes(netmask.s6_addr[i]);
                    }
                }
                    break;
                default:
                    break;
            }

            if(addr) {
                result.emplace(next->ifa_name, std::make_pair(addr, prefix_len));
            }
        }
    } catch (...) {
        ACID_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return !result.empty();
}

bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >&result
                                    ,const std::string& iface, int family) {
    if(iface.empty() || iface == "*") {
        if(family == AF_INET || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
        }
        if(family == AF_INET6 || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
        }
        return true;
    }

    std::multimap<std::string, std::pair<Address::ptr, uint32_t> > results;

    if(!GetInterfaceAddresses(results, family)) {
        return false;
    }

    auto its = results.equal_range(iface);
    for(; its.first != its.second; ++its.first) {
        result.push_back(its.first->second);
    }
    return !result.empty();
}


int Address::getFamily() const{
    return getAddr()->sa_family;
}

std::string Address::toString() {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address &rhs) const {
    socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
    int result = memcmp(getAddr(), rhs.getAddr(), minlen);
    if(result < 0) {
        return true;
    } else if(result > 0) {
        return false;
    } else if(getAddrLen() < rhs.getAddrLen()) {
        return true;
    }
    return false;
}

bool Address::operator==(const Address &rhs) const {
    return getAddrLen() == rhs.getAddrLen() && memcmp(getAddr(),rhs.getAddr(),getAddrLen());
}

bool Address::operator!=(const Address &rhs) const {
    return !(*this == rhs);
}

IPAddress::ptr IPAddress::Create(const char *host_name, uint16_t port) {
    addrinfo hints, *results;
    memset(&hints, 0, sizeof(addrinfo));

    //hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(host_name, NULL, &hints, &results);
    if(error) {
        ACID_LOG_DEBUG(g_logger) << "IPAddress::Create(" << host_name
                                  << ", " << port << ") error=" << error
                                  << " errstr=" << gai_strerror(error);
        return nullptr;
    }

    try {
        Address::ptr address = Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen);
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(address);
        if(result) {
            result->setPort(port);
        }
        freeaddrinfo(results);
        return result;
    } catch (...) {
        freeaddrinfo(results);
        return nullptr;
    }
}

IPv4Address::ptr IPv4Address::Create(const char *ip, uint16_t port) {
    IPv4Address::ptr rt(new IPv4Address);
    rt->m_addr.sin_port = EndianCast(port);
    int result = inet_pton(AF_INET, ip, &rt->m_addr.sin_addr);
    if(result <= 0) {
        ACID_LOG_DEBUG(g_logger) << "IPv4Address::Create(" << ip << ", "
                                  << port << ") rt=" << result << " errno=" << errno
                                  << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv4Address::IPv4Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
}

IPv4Address::IPv4Address(const sockaddr_in& address) {
    m_addr = address;
}

IPv4Address::IPv4Address(uint32_t ip, uint16_t port) {
    memset(&m_addr, 0, sizeof (m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = HostToNetwork(port);
    m_addr.sin_addr.s_addr = HostToNetwork(ip);

}

const sockaddr *IPv4Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

const socklen_t IPv4Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &IPv4Address::insert(std::ostream &out) {
    uint32_t addr = NetworkToHost(m_addr.sin_addr.s_addr);
    out << ((addr >> 24) & 0xFF) << '.'
        << ((addr >> 16) & 0xFF) << '.'
        << ((addr >> 8) & 0xFF) << '.'
        << (addr & 0xFF) ;
    out << ':' << NetworkToHost(m_addr.sin_port);
    return out;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in broadAddr(m_addr);
    broadAddr.sin_addr.s_addr |= EndianCast(CreateMask<uint32_t>(prefix_len));
    IPv4Address::ptr res(new IPv4Address(broadAddr));
    return res;
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in netAddr(m_addr);
    netAddr.sin_addr.s_addr &= EndianCast(CreateMask<uint32_t>(prefix_len));
    IPv4Address::ptr res(new IPv4Address(netAddr));
    return res;
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in subnet(m_addr);
    memset(&subnet, 0, sizeof(sockaddr));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~EndianCast(CreateMask<uint32_t>(prefix_len));
    IPv4Address::ptr res(new IPv4Address(subnet));
    return res;
}

uint32_t IPv4Address::getPort() const {
    return NetworkToHost(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t port) {
    m_addr.sin_port = HostToNetwork(port);
}

sockaddr *IPv4Address::getAddr() {
    return (sockaddr*)&m_addr;
}

IPv6Address::ptr IPv6Address::Create(const char *ip, uint16_t port) {
    IPv6Address::ptr rt(new IPv6Address);
    rt->m_addr.sin6_port = EndianCast(port);
    int result = inet_pton(AF_INET, ip, &rt->m_addr.sin6_addr);
    if(result <= 0) {
        ACID_LOG_DEBUG(g_logger) << "IPv6Address::Create(" << ip << ", "
                                 << port << ") rt=" << result << " errno=" << errno
                                 << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}


IPv6Address::IPv6Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& addr) {
    m_addr = addr;
}

IPv6Address::IPv6Address(const uint8_t ip[16], uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = HostToNetwork(port);
    memcpy(&m_addr.sin6_addr.s6_addr, ip, 16);
}

const sockaddr *IPv6Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

const socklen_t IPv6Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &IPv6Address::insert(std::ostream &os) {
    uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    os << "[";
    bool used_zeros = false;
    for(size_t i = 0; i < 8; ++i) {
        if(addr[i] == 0 && !used_zeros) {
            continue;
        }
        if(i && addr[i - 1] == 0 && !used_zeros) {
            os << ":";
            used_zeros = true;
        }
        if(i) {
            os << ":";
        }
        os << std::hex << (int)NetworkToHost(addr[i]) << std::dec;
    }

    if(!used_zeros && addr[7] == 0) {
        os << "::";
    }

    os << "]:" << NetworkToHost(m_addr.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] |=
            CreateMask<uint8_t>(prefix_len % 8);
    for(int i = prefix_len / 8 + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] &=
            CreateMask<uint8_t>(prefix_len % 8);
    for(int i = prefix_len / 8 + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0x00;
    }
    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len /8] =
            ~CreateMask<uint8_t>(prefix_len % 8);

    for(uint32_t i = 0; i < prefix_len / 8; ++i) {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const {
    return NetworkToHost(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t port) {
    m_addr.sin6_port = HostToNetwork(port);
}

sockaddr *IPv6Address::getAddr() {
    return (sockaddr*)&m_addr;
}


static constexpr size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_len = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string &path) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_len = path.size() + 1;

    if(!path.empty() && path[0] == '\0') {
        --m_len;
    }

    if(m_len > sizeof(m_addr.sun_path)) {
        throw std::logic_error("path too long");
    }
    memcpy(m_addr.sun_path, path.c_str(), m_len);
    m_len += offsetof(sockaddr_un, sun_path);
}
const sockaddr *UnixAddress::getAddr() const {
    return (sockaddr*)&m_addr;
}

const socklen_t UnixAddress::getAddrLen() const {
    return m_len;
}

std::ostream &UnixAddress::insert(std::ostream &os) {
    if(m_len > offsetof(sockaddr_un, sun_path)
       && m_addr.sun_path[0] == '\0') {
        return os << "\\0" << std::string(m_addr.sun_path + 1,
                                          m_len - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << m_addr.sun_path;
}

sockaddr *UnixAddress::getAddr() {
    return (sockaddr*)&m_addr;
}

void UnixAddress::setAddrLen(uint32_t  len) {
    m_len = len;
}

UnknownAddress::UnknownAddress(int family) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) {
    m_addr = addr;
}
const sockaddr *UnknownAddress::getAddr() const {
    return (sockaddr*) &m_addr;
}

const socklen_t UnknownAddress::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &UnknownAddress::insert(std::ostream &os) {
    os << "[ UnknownAddress family=" << m_addr.sa_family << " ]";
    return os;
}

sockaddr *UnknownAddress::getAddr() {
    return (sockaddr*)&m_addr;
}


}