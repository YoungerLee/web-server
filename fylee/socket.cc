#include "socket.h"
#include "fdmanager.h"
#include "log.h"
#include "macro.h"
#include <limits.h>

namespace fylee {

static fylee::Logger::ptr g_logger = LOG_NAME("system");

Socket::ptr Socket::CreateTCP(fylee::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(fylee::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
    sock->newSock();
    sock->isConnected_ = true;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
    Socket::ptr sock(new Socket(IPv4, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
    Socket::ptr sock(new Socket(IPv4, UDP, 0));
    sock->newSock();
    sock->isConnected_ = true;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
    Socket::ptr sock(new Socket(IPv6, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket6() {
    Socket::ptr sock(new Socket(IPv6, UDP, 0));
    sock->newSock();
    sock->isConnected_ = true;
    return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket() {
    Socket::ptr sock(new Socket(UNIX, TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket() {
    Socket::ptr sock(new Socket(UNIX, UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol)
    :sockfd_(-1)
    ,family_(family)
    ,type_(type)
    ,protocol_(protocol)
    ,isConnected_(false) {
}

Socket::~Socket() {
    close();
}

int64_t Socket::getSendTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd_);
    if(ctx) {
        return ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(int64_t v) {
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd_);
    if(ctx) {
        return ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(int64_t v) {
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, socklen_t* len) {
    int rt = getsockopt(sockfd_, level, option, result, (socklen_t*)len);
    if(rt) {
        LOG_DEBUG(g_logger) << "getOption sock=" << sockfd_
            << " level=" << level << " option=" << option
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, const void* result, socklen_t len) {
    if(setsockopt(sockfd_, level, option, result, (socklen_t)len)) {
        LOG_DEBUG(g_logger) << "setOption sock=" << sockfd_
            << " level=" << level << " option=" << option
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setNonBlock() {
    int flag = fcntl(sockfd_, F_GETFL, 0);
    if (flag == -1) return false;
    flag |= O_NONBLOCK;
    if (fcntl(sockfd_, F_SETFL, flag) == -1) return false;
    return true;
}
Socket::ptr Socket::accept() {
    Socket::ptr sock(new Socket(family_, type_, protocol_));
    int newsock = ::accept(sockfd_, nullptr, nullptr);
    if(newsock == -1) {
        LOG_ERROR(g_logger) << "accept(" << sockfd_ << ") errno="
            << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    if(sock->init(newsock)) {
        return sock;
    }
    return nullptr;
}

bool Socket::init(int sock) {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock, true);
    if(ctx && ctx->isSocket() && !ctx->isClose()) {
        sockfd_ = sock;
        isConnected_ = true;
        initSock();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

bool Socket::bind(const Address::ptr addr) {
    //localAddress_ = addr;
    if(!isValid()) {
        newSock();
        if(UNLIKELY(!isValid())) {
            return false;
        }
    }

    if(UNLIKELY(addr->getFamily() != family_)) {
        LOG_ERROR(g_logger) << "bind sock.family("
            << family_ << ") addr.family(" << addr->getFamily()
            << ") not equal, addr=" << addr->toString();
        return false;
    }

    UnixAddress::ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
    if(uaddr) {
        Socket::ptr sock = Socket::CreateUnixTCPSocket();
        if(sock->connect(uaddr)) {
            return false;
        } else {
            fylee::FSUtil::Unlink(uaddr->getPath(), true);
        }
    }
    
    if(::bind(sockfd_, addr->getAddr(), addr->getAddrLen())) {
        LOG_ERROR(g_logger) << "bind error errrno=" << errno
            << " errstr=" << strerror(errno);
        return false;
    }
    getLocalAddress();
    return true;
}

bool Socket::reconnect(uint64_t timeout_ms) {
    if(!remoteAddress_) {
        LOG_ERROR(g_logger) << "reconnect remoteAddress_ is null";
        return false;
    }
    localAddress_.reset();
    return connect(remoteAddress_, timeout_ms);
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    remoteAddress_ = addr;
    if(!isValid()) {
        newSock();
        if(UNLIKELY(!isValid())) {
            return false;
        }
    }

    if(UNLIKELY(addr->getFamily() != family_)) {
        LOG_ERROR(g_logger) << "connect sock.family("
            << family_ << ") addr.family(" << addr->getFamily()
            << ") not equal, addr=" << addr->toString();
        return false;
    }
    
    if(::connect(sockfd_, addr->getAddr(), addr->getAddrLen())) {
        LOG_ERROR(g_logger) << "sock=" << sockfd_ << " connect(" << addr->toString()
            << ") error errno=" << errno << " errstr=" << strerror(errno);
        close();
        return false;
    }
    isConnected_ = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog) {
    if(!isValid()) {
        LOG_ERROR(g_logger) << "listen error sock=-1";
        return false;
    }
    if(::listen(sockfd_, backlog)) {
        LOG_ERROR(g_logger) << "listen error errno=" << errno
            << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close() {
    if(!isConnected_ && sockfd_ == -1) {
        return true;
    }
    isConnected_ = false;
    if(sockfd_ != -1) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
    return false;
}

int Socket::send(const void* buffer, size_t length, int flags) {
    if(isConnected()) {
        return ::send(sockfd_, buffer, length, flags);
    }
    return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::sendmsg(sockfd_, &msg, flags);
    }
    return -1;
}

int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
    if(isConnected()) {
        return ::sendto(sockfd_, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
    if(isConnected()) {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(sockfd_, &msg, flags);
    }
    return -1;
}

int Socket::recv(void* buffer, size_t length, int flags) {
    if(isConnected()) {
        return ::recv(sockfd_, buffer, length, flags);
    }
    return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags) {
    if(isConnected()) {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::recvmsg(sockfd_, &msg, flags);
    }
    return -1;
}

int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
    if(isConnected()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(sockfd_, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
    if(isConnected()) {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(sockfd_, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::getRemoteAddress() {
    if(remoteAddress_) {
        return remoteAddress_;
    }

    Address::ptr result;
    switch(family_) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(family_));
            break;
    }
    socklen_t addrlen = result->getAddrLen();
    if(getpeername(sockfd_, result->getAddr(), &addrlen)) {
        LOG_ERROR(g_logger) << "getpeername error sock=" << sockfd_
            << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(family_));
    }
    if(family_ == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }
    remoteAddress_ = result;
    return remoteAddress_;
}

Address::ptr Socket::getLocalAddress() {
    if(localAddress_) {
        return localAddress_;
    }

    Address::ptr result;
    switch(family_) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(family_));
            break;
    }
    socklen_t addrlen = result->getAddrLen();
    if(getsockname(sockfd_, result->getAddr(), &addrlen)) {
        LOG_ERROR(g_logger) << "getsockname error sock=" << sockfd_
            << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnknownAddress(family_));
    }
    if(family_ == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }
    localAddress_ = result;
    return localAddress_;
}

bool Socket::isValid() const {
    return sockfd_ != -1;
}

int Socket::getError() {
    int error = 0;
    socklen_t len = sizeof(error);
    if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
        error = errno;
    }
    return error;
}

std::ostream& Socket::dump(std::ostream& os) const {
    os << "[Socket sock=" << sockfd_
       << " is_connected=" << isConnected_
       << " family=" << family_
       << " type=" << type_
       << " protocol=" << protocol_;
    if(localAddress_) {
        os << " local_address=" << localAddress_->toString();
    }
    if(remoteAddress_) {
        os << " remote_address=" << remoteAddress_->toString();
    }
    os << "]";
    return os;
}

std::string Socket::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

bool Socket::shutdownWrite() {
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR(g_logger) << "shutdown write fd = " << sockfd_ 
                    << " failed: " << " errno=" << errno 
                    << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

void Socket::initSock() {
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if(type_ == SOCK_STREAM) {
        setOption(IPPROTO_TCP, TCP_NODELAY, val);
    }
}

void Socket::newSock() {
    sockfd_ = socket(family_, type_, protocol_);
    if(LIKELY(sockfd_ != -1)) {
        initSock();
    } else {
        LOG_ERROR(g_logger) << "socket(" << family_
            << ", " << type_ << ", " << protocol_ << ") errno="
            << errno << " errstr=" << strerror(errno);
    }
}

std::ostream& operator<<(std::ostream& os, const Socket& sock) {
    return sock.dump(os);
}

}
