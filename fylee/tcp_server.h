#ifndef __FYLEE_TCP_SERVER_H_
#define __FYLEE_TCP_SERVER_H_

#include <memory>
#include <atomic>
#include <string>
#include <stdint.h>
#include <map>
#include "noncopyable.h"
#include "mutex.h"

namespace fylee {
class EventLoop;
class EventLoopThreadPool;
class Channel;
class Connection;
class Address;
class Socket;
class Buffer;

typedef std::function<void (const std::shared_ptr<Connection>)> ConnectionCallback;
typedef std::function<void (const std::shared_ptr<Connection>)> CloseCallback;
typedef std::function<void (const std::shared_ptr<Connection>)> WriteCompleteCallback;
typedef std::function<void (const std::shared_ptr<Connection>, uint64_t)> MessageCallback;

class Acceptor : public std::enable_shared_from_this<Acceptor>, Noncopyable {
public:
    typedef std::shared_ptr<Acceptor> ptr;
    typedef std::function<void (const std::shared_ptr<Socket>)> NewConnectionCallback;

    Acceptor(EventLoop* loop, const std::shared_ptr<Address> listenAddr);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) { newConnectionCallback_ = cb; }

    bool isListenning() const { return listenning_; }
    void listen();

private:
    void handleRead();
    EventLoop* loop_;
    std::shared_ptr<Socket> acceptSocket_;
    std::shared_ptr<Channel> acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};

class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    typedef std::function<void(EventLoop*)> ThreadInitCallback;
    typedef Mutex MutexType;

    TcpServer(EventLoop* loop, const std::shared_ptr<Address> addr, const std::string& name);
    virtual ~TcpServer(); 

    std::string getName() const { return name_; }

    virtual void setName(const std::string& name) { name_ = name;}

    EventLoop* getLoop() const { return loop_; }

    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback& cb);

    std::shared_ptr<EventLoopThreadPool> getThreadPool() { return threadPool_; }

    void start();

    void setConnectionCallback(const ConnectionCallback& cb); 

    void setMessageCallback(const MessageCallback& cb); 

    void setWriteCompleteCallback(const WriteCompleteCallback& cb); 

private:
    void newConnection(const std::shared_ptr<Socket> addr);
    void removeConnection(const std::shared_ptr<Connection> conn);
    void removeConnectionInLoop(const std::shared_ptr<Connection> conn);
private:
    typedef std::map<std::string, std::shared_ptr<Connection>> ConnectionMap;

    mutable MutexType mutex_;
    EventLoop* loop_;  // the acceptor loop
    std::string name_;
    Acceptor::ptr acceptor_; 
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    std::atomic_bool started_;
    int nextConnId_;
    ConnectionMap connections_;
};

}

#endif