#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "connection.h"
#include "tcp_server.h"
#include "log.h"
#include "weakcb.h"
#include "macro.h"
#include "eventloop.h"
#include "eventloopthreadpool.h"
#include "channel.h"
#include "socket.h"
#include "buffer.h"
#include "socket_stream.h"
#include "http/http_session.h"

namespace fylee {
using namespace std::placeholders;
static fylee::Logger::ptr g_logger = LOG_NAME("system");
Connection::Connection(EventLoop* loop,
                       const std::string& name,
                       const Socket::ptr socket, 
                       const SocketStream::ptr stream) 
    :loop_(loop),
    name_(name),
    socket_(socket),
    stream_(stream),
    state_(kConnecting),
    channel_(new Channel(loop, socket_->getSocket())),
    highWaterMark_(64 * 1024 * 1024), 
    inputBuffer_(new Buffer), 
    outputBuffer_(new Buffer) {
    
    channel_->setReadCallback(std::bind(&Connection::handleRead, this, _1));
    channel_->setWriteCallback(std::bind(&Connection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&Connection::handleClose, this));
    channel_->setErrorCallback(std::bind(&Connection::handleError, this));
    LOG_DEBUG(g_logger) << "Connection::ctor[" <<  name_ << "] at " << this
                << " fd=" << socket_->getSocket();
}

Connection::~Connection() {
    LOG_DEBUG(g_logger) << "Connection::dtor[" <<  name_ << "] at " << this
                << " fd=" << channel_->getFd()
                << " state=" << stateToString();
    ASSERT(state_ == kDisconnected);
}

void Connection::send(const void* data, int len) {
    std::string msg(static_cast<const char*>(data), len);
    send(msg);
}

void Connection::send(const std::string& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        } else {
            void (Connection::*fp)(const std::string& message) = &Connection::sendInLoop;
            loop_->runInLoop(std::bind(fp, shared_from_this(), message));
        }
    }
}

void Connection::send(Buffer::ptr buf) {
    std::string msg = buf->toString();
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(msg);
        } else {
            void (Connection::*fp)(const std::string& message) = &Connection::sendInLoop;
            loop_->runInLoop(std::bind(fp, shared_from_this(), msg));
        }
    }
}

void Connection::sendInLoop(const std::string& message) {
    sendInLoop(message.data(), message.size());
}

void Connection::sendInLoop(const void* data, size_t len) {
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    if (state_ == kDisconnected) {
        LOG_WARN(g_logger) << "disconnected, give up writing";
        return;
    }

    if (!channel_->isWriting() && outputBuffer_->getReadSize() == 0) {
        nwrote = stream_->write(data, len);
        if (nwrote >= 0) { 
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) { // 一次发完
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR(g_logger) << "Connection::sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET)  {
                    faultError = true;
                }
            }
        }
    }

    ASSERT(remaining <= len);
    if (!faultError && remaining > 0) { // 继续写完剩下部分
        size_t oldLen = outputBuffer_->getReadSize();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_->write(static_cast<const char*>(data) + nwrote, remaining);
        channel_->enableWriting();
    }
}

void Connection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&Connection::shutdownInLoop, this));
    }
}

void Connection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void Connection::forceClose() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&Connection::forceCloseInLoop, this));
    }
}

void Connection::forceCloseWithDelay(uint64_t seconds) {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        loop_->runAfter(
            seconds,
            makeWeakCallback(shared_from_this(), &Connection::forceClose)); 
    }
}

void Connection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        handleClose();
    }
}

const char* Connection::stateToString() const {
    switch (state_) {
#define XX(name) \
    case name: \
        return #name; 

    XX(kDisconnected)
    XX(kConnecting)
    XX(kConnected)
    XX(kDisconnecting)
#undef XX
    default:
    return "unknown state";
    }
}

void Connection::startRead() {
    loop_->runInLoop(std::bind(&Connection::startReadInLoop, shared_from_this()));
}

void Connection::startReadInLoop() {
    loop_->assertInLoopThread();
    if (!reading_ || !channel_->isReading()) {
        channel_->enableReading();
        reading_ = true;
    }
}

void Connection::stopRead() {
    loop_->runInLoop(std::bind(&Connection::stopReadInLoop, shared_from_this()));
}

void Connection::stopReadInLoop() {
    loop_->assertInLoopThread();
    if (channel_->isReading()) {
        channel_->disableReading();
    }
}

void Connection::connectEstablished() {
    loop_->assertInLoopThread();
    ASSERT(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();
    connectionCallback_(shared_from_this());
}

void Connection::connectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();
    }
    channel_->remove();
}

bool Connection::isReading() const {
    return reading_;
}

void Connection::handleRead(uint64_t receiveTime) {
    loop_->assertInLoopThread();
    messageCallback_(shared_from_this(), receiveTime);
    channel_->enableReading();
}

void Connection::handleWrite() {
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        int rt = stream_->write(outputBuffer_, outputBuffer_->getReadSize());
        if (rt > 0) {
            std::shared_ptr<char> buffer(new char[rt], 
                                        [](char* ptr) { delete[] ptr; });
            outputBuffer_->read(buffer.get(), rt);
            if (outputBuffer_->getReadSize() == 0) {
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR(g_logger)<< "TcpConnection::handleWrite";
        }
    } else {
        LOG_INFO(g_logger) << "Connection fd = " << channel_->getFd()
                << " is down, no more writing";
    }
}

void Connection::handleClose() {
    loop_->assertInLoopThread();
    LOG_INFO(g_logger) << "fd = " << channel_->getFd() << " state = " << stateToString();
    ASSERT(state_ == kConnected || state_ == kDisconnecting);
    // we don't close fd, leave it to dtor, so we can find leaks easily.
    setState(kDisconnected);
    channel_->disableAll();
    // must be the last line
    closeCallback_(shared_from_this());
}

void Connection::handleError() {
    int err = socket_->getError();
    LOG_ERROR(g_logger) << "Connection::handleError [" << name_
                << "] - SO_ERROR = " << err << " " << strerror(err);
}

}