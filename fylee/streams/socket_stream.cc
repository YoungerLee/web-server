#include "socket_stream.h"
#include "fylee/util.h"

namespace fylee {

SocketStream::SocketStream(Socket::ptr sock, bool owner)
    :socket_(sock)
    ,owner_(owner) {
}

SocketStream::~SocketStream() {
    if(owner_ && socket_) {
        socket_->close();
    }
}

bool SocketStream::isConnected() const {
    return socket_ && socket_->isConnected();
}

int SocketStream::read(void* buffer, size_t length) {
    if(!isConnected()) {
        return -1;
    }
    return socket_->recv(buffer, length);
}

int SocketStream::read(Buffer::ptr ba, size_t length) {
    if(!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getWriteBuffers(iovs, length);
    int rt = socket_->recv(&iovs[0], iovs.size());
    if(rt > 0) {
        ba->setPosition(ba->getPosition() + rt);
    }
    return rt;
}

int SocketStream::write(const void* buffer, size_t length) {
    if(!isConnected()) {
        return -1;
    }
    return socket_->send(buffer, length);
}

int SocketStream::write(Buffer::ptr ba, size_t length) {
    if(!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    ba->getReadBuffers(iovs, length);
    int rt = socket_->send(&iovs[0], iovs.size());
    if(rt > 0) {
        ba->setPosition(ba->getPosition() + rt);
    }
    return rt;
}

void SocketStream::close() {
    if(socket_) {
        socket_->close();
    }
}

Address::ptr SocketStream::getRemoteAddress() {
    if(socket_) {
        return socket_->getRemoteAddress();
    }
    return nullptr;
}

Address::ptr SocketStream::getLocalAddress() {
    if(socket_) {
        return socket_->getLocalAddress();
    }
    return nullptr;
}

std::string SocketStream::getRemoteAddressString() {
    auto addr = getRemoteAddress();
    if(addr) {
        return addr->toString();
    }
    return "";
}

std::string SocketStream::getLocalAddressString() {
    auto addr = getLocalAddress();
    if(addr) {
        return addr->toString();
    }
    return "";
}

}
