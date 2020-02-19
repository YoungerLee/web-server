#include "address.h"
#include "log.h"
#include <sstream>
#include <netdb.h>
#include <ifaddrs.h>
#include <stddef.h>

namespace fylee {

static fylee::Logger::ptr g_logger = LOG_NAME("system");

template<class T>
static T CreateMask(uint32_t bits) {
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

template<class T>
static uint32_t CountBytes(T value) {
    uint32_t result = 0;
    for(; value; ++result) {
        value &= value - 1;
    }
    return result;
}

Address::ptr Address::LookupAny(const std::string& host,
                                int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)) {
        return result[0];
    }
    return nullptr;
}

IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host,
                                int family, int type, int protocol) {
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
    struct addrinfo hints, *results, *next;
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

    // 检查 ipv6address serivce 
    // e.g. [3ff3:b80:1f8d:2:204:acff:fe17:bf38]:21
    if(!host.empty() && host[0] == '[') {
        auto ipv6end = host.find_first_of(']');
        if(ipv6end != host.npos) {
            if(ipv6end + 2 != host.npos && host[ipv6end + 1] == ':') {
                service = host.substr(ipv6end + 2).c_str();
            }
        }
        node = host.substr(1, ipv6end - 1);
    }
  
    // 检查 ipv4address serivce 
    // e.g. www.baidu.com:21
    if(node.empty()) {
        auto split = host.find_first_of(':');
        if(split != host.npos && split + 1 != host.npos) {
            service = host.substr(split + 1).c_str();
            node = host.substr(0, split);
        }
    }
    
    if(node.empty()) {
        node = host;
    }
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if(error) {
        LOG_DEBUG(g_logger) << "Address::Lookup getaddress(" << host << ", "
            << family << ", " << type << ") err=" << error << " errstr="
            << gai_strerror(error);
        return false;
    }

    next = results;
    while(next) {
        result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        //LOG_INFO(g_logger) << ((sockaddr_in*)next->ai_addr)->sin_addr.s_addr;
        next = next->ai_next;
    }

    freeaddrinfo(results);
    return !result.empty();
}

bool Address::GetInterfaceAddresses(std::multimap<std::string, 
                    std::pair<Address::ptr, uint32_t> >& result,
                    int family) {
    struct ifaddrs *next, *results;
    if(getifaddrs(&results) != 0) {
        LOG_DEBUG(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
            " err=" << errno << " errstr=" << strerror(errno);
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
                result.insert(std::make_pair(next->ifa_name,
                            std::make_pair(addr, prefix_len)));
            }
        }
    } catch (...) {
        LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
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

    std::multimap<std::string
          ,std::pair<Address::ptr, uint32_t> > results;

    if(!GetInterfaceAddresses(results, family)) {
        return false;
    }

    auto its = results.equal_range(iface);
    for(; its.first != its.second; ++its.first) {
        result.push_back(its.first->second);
    }
    return !result.empty();
}

int Address::getFamily() const {
    return getAddr()->sa_family;
}

std::string Address::toString() const {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
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

bool Address::operator<(const Address& rhs) const {
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

bool Address::operator==(const Address& rhs) const {
    return getAddrLen() == rhs.getAddrLen()
        && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const {
    return !(*this == rhs);
}

IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
    struct addrinfo hints, *results;
    memset(&hints, 0, sizeof(addrinfo));

    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(address, NULL, &hints, &results);
    if(error) {
        LOG_DEBUG(g_logger) << "IPAddress::Create(" << address
            << ", " << port << ") error=" << error
            << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }

    try {
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
                Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
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

IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
    IPv4Address::ptr rt(new IPv4Address);
    rt->addr_.sin_port = htons(port);
    int result = inet_pton(AF_INET, address, &rt->addr_.sin_addr);
    if(result <= 0) {
        LOG_DEBUG(g_logger) << "IPv4Address::Create(" << address << ", "
                << port << ") rt=" << result << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv4Address::IPv4Address(const sockaddr_in& address) {
    addr_ = address;
}

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = htonl(address);
}

sockaddr* IPv4Address::getAddr() {
    return (sockaddr*)&addr_;
}

const sockaddr* IPv4Address::getAddr() const {
    return (sockaddr*)&addr_;
}

socklen_t IPv4Address::getAddrLen() const {
    return sizeof(addr_);
}

std::ostream& IPv4Address::insert(std::ostream& os) const {
    uint32_t addr = ntohl(addr_.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "."
       << ((addr >> 16) & 0xff) << "."
       << ((addr >> 8) & 0xff) << "."
       << (addr & 0xff);
    os << ":" << ntohs(addr_.sin_port);
    return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }

    sockaddr_in baddr(addr_);
    baddr.sin_addr.s_addr |= htonl(
            CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::networdAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }

    sockaddr_in baddr(addr_);
    baddr.sin_addr.s_addr &= htonl(
            CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~htonl(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::getPort() const {
    return ntohs(addr_.sin_port);
}

void IPv4Address::setPort(uint16_t v) {
    addr_.sin_port = htons(v);
}

IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
    IPv6Address::ptr rt(new IPv6Address);
    rt->addr_.sin6_port = htons(port);
    int result = inet_pton(AF_INET6, address, &rt->addr_.sin6_addr);
    if(result <= 0) {
        LOG_DEBUG(g_logger) << "IPv6Address::Create(" << address << ", "
                << port << ") rt=" << result << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv6Address::IPv6Address() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& address) {
    addr_ = address;
}

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
    addr_.sin6_port = htons(port);
    memcpy(&addr_.sin6_addr.s6_addr, address, 16);
}

sockaddr* IPv6Address::getAddr() {
    return (sockaddr*)&addr_;
}

const sockaddr* IPv6Address::getAddr() const {
    return (sockaddr*)&addr_;
}

socklen_t IPv6Address::getAddrLen() const {
    return sizeof(addr_);
}

std::ostream& IPv6Address::insert(std::ostream& os) const {
    os << "[";
    uint16_t* addr = (uint16_t*)addr_.sin6_addr.s6_addr;
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
        os << std::hex << (int)ntohl(addr[i]) << std::dec;
    }

    if(!used_zeros && addr[7] == 0) {
        os << "::";
    }

    os << "]:" << ntohs(addr_.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
    sockaddr_in6 baddr(addr_);
    baddr.sin6_addr.s6_addr[prefix_len / 8] |= 
        CreateMask<uint8_t>(prefix_len % 8);
    for(int i = prefix_len / 8 + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networdAddress(uint32_t prefix_len) {
    sockaddr_in6 baddr(addr_);
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
    subnet.sin6_addr.s6_addr[prefix_len / 8] =
        ~CreateMask<uint8_t>(prefix_len % 8);

    for(uint32_t i = 0; i < prefix_len / 8; ++i) {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const {
    return ntohs(addr_.sin6_port);
}

void IPv6Address::setPort(uint16_t v) {
    addr_.sin6_port = htons(v);
}

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    length_ = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    length_ = path.size() + 1;

    if(!path.empty() && path[0] == '\0') {
        --length_;
    }

    if(length_ > sizeof(addr_.sun_path)) {
        throw std::logic_error("path too long");
    }
    memcpy(addr_.sun_path, path.c_str(), length_);
    length_ += offsetof(sockaddr_un, sun_path);
}

void UnixAddress::setAddrLen(uint32_t v) {
    length_ = v;
}

sockaddr* UnixAddress::getAddr() {
    return (sockaddr*)&addr_;
}

const sockaddr* UnixAddress::getAddr() const {
    return (sockaddr*)&addr_;
}

socklen_t UnixAddress::getAddrLen() const {
    return length_;
}

std::string UnixAddress::getPath() const {
    std::stringstream ss;
    if(length_ > offsetof(sockaddr_un, sun_path)
            && addr_.sun_path[0] == '\0') {
        ss << "\\0" << std::string(addr_.sun_path + 1,
                length_ - offsetof(sockaddr_un, sun_path) - 1);
    } else {
        ss << addr_.sun_path;
    }
    return ss.str();
}

std::ostream& UnixAddress::insert(std::ostream& os) const {
    if(length_ > offsetof(sockaddr_un, sun_path)
            && addr_.sun_path[0] == '\0') {
        return os << "\\0" << std::string(addr_.sun_path + 1,
                length_ - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << addr_.sun_path;
}

UnknownAddress::UnknownAddress(int family) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) {
    addr_ = addr;
}

sockaddr* UnknownAddress::getAddr() {
    return (sockaddr*)&addr_;
}

const sockaddr* UnknownAddress::getAddr() const {
    return &addr_;
}

socklen_t UnknownAddress::getAddrLen() const {
    return sizeof(addr_);
}

std::ostream& UnknownAddress::insert(std::ostream& os) const {
    os << "[UnknownAddress family=" << addr_.sa_family << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return addr.insert(os);
}

}
