#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "tcp_server.h"
#include "log.h"
#include "weakcb.h"
#include "macro.h"
#include "eventloop.h"
#include "eventloopthreadpool.h"
#include "channel.h"
#include "socket.h"
#include "buffer.h"
#include "connection.h"
#include "http/http_session.h"

namespace fylee {
static fylee::Logger::ptr g_logger = LOG_NAME("system");
using namespace std::placeholders;

Acceptor::Acceptor(EventLoop* loop, const Address::ptr addr) 
    :loop_(loop),
     acceptSocket_(Socket::CreateTCP(addr)),
     listenning_(false) {
    acceptSocket_->bind(addr);
    acceptChannel_ = std::make_shared<Channel>(loop_, acceptSocket_->getSocket());
    acceptChannel_->setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_->disableAll();
    acceptChannel_->remove();
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listenning_ = true;
    acceptSocket_->listen();
    acceptSocket_->setNonBlock();
    acceptChannel_->enableReading();
}

void Acceptor::handleRead() {
    loop_->assertInLoopThread();
    Socket::ptr client;
    while ((client = acceptSocket_->accept()) != nullptr) {
        if (newConnectionCallback_) {
            newConnectionCallback_(client);
        } else {
            client->close();
        }
    }
    LOG_ERROR(g_logger) << "in Acceptor::handleRead";
}

TcpServer::TcpServer(EventLoop* loop, 
                     const Address::ptr addr, 
                     const std::string& name)
    :loop_(loop),
     name_(name),
     acceptor_(new Acceptor(loop, addr)),
     threadPool_(new EventLoopThreadPool(loop, name_)),
     connectionCallback_([](const Connection::ptr conn) {
         LOG_INFO(g_logger) << conn->getSocket()->toString() << " is " 
                            << (conn->isConnected() ? "UP" : "DOWN");
     }),
     messageCallback_([](const Connection::ptr conn, 
                         uint64_t receiveTime) { 
        Buffer::ptr inBuff = conn->inputBuffer();
        inBuff->setPosition(0);
     }),
     started_(false),
     nextConnId_(1) {
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, _1));
}

TcpServer::~TcpServer() {
    loop_->assertInLoopThread();
    LOG_INFO(g_logger) << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for (auto& item : connections_) {
        auto conn = item.second;
        item.second.reset();
        conn->getLoop()->runInLoop(std::bind(&Connection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads) {
    ASSERT(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::setThreadInitCallback(const ThreadInitCallback& cb) {
    MutexType::Lock lock(mutex_);
    threadInitCallback_ = cb;
}

void TcpServer::setConnectionCallback(const ConnectionCallback& cb) {
    MutexType::Lock lock(mutex_);
    connectionCallback_ = cb;
}

void TcpServer::setMessageCallback(const MessageCallback& cb) {
    MutexType::Lock lock(mutex_);
    messageCallback_ = cb;
}

void TcpServer::setWriteCompleteCallback(const WriteCompleteCallback& cb) {
    MutexType::Lock lock(mutex_);
    writeCompleteCallback_ = cb;
}

void TcpServer::start() {
    if (!started_) {
        started_ = true;
        threadPool_->start(threadInitCallback_);
        ASSERT(!acceptor_->isListenning());
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_));
    }
}

void TcpServer::newConnection(const Socket::ptr client) {
    loop_->assertInLoopThread();
    EventLoop* ioLoop = threadPool_->getNextLoop();
    std::stringstream ss;
    ss << name_ << " " << client->toString() << " " << nextConnId_;
    std::string connName = ss.str();
    std::transform(connName.begin(), connName.end(), connName.begin(), ::tolower);
    ++nextConnId_;
    LOG_INFO(g_logger) << "TcpServer::newConnection [" << name_
            << "] - new connection [" << connName
            << "] from " << client->toString();
    SocketStream::ptr stream;
    if (name_ == "HttpServer")
        stream = std::make_shared<http::HttpSession>(client, false);
    else 
        stream = std::make_shared<SocketStream>(client, false);
    Connection::ptr conn = std::make_shared<Connection>(ioLoop, connName, client, stream);
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, shared_from_this(), _1)); 
    ioLoop->runInLoop(std::bind(&Connection::connectEstablished, conn));
}

void TcpServer::removeConnection(const Connection::ptr conn) {
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, shared_from_this(), conn));
}

void TcpServer::removeConnectionInLoop(const Connection::ptr conn) {
    loop_->assertInLoopThread();
    LOG_INFO(g_logger) << "TcpServer::removeConnectionInLoop [" << name_
            << "] - connection " << conn->getName();
    size_t n = connections_.erase(conn->getName());
    ASSERT(n == 1);
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&Connection::connectDestroyed, conn));
}
}
