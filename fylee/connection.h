#ifndef __FYLEE_CONNECTION_H_
#define __FYLEE_CONNECTION_H_
#include <memory>
#include <atomic>
#include <string>
#include <stdint.h>
#include <functional>
#include "mutex.h"
#include "noncopyable.h"

namespace fylee {
class EventLoop;
class EventLoopThreadPool;
class Channel;
class Connection;
class Socket;
class Buffer;
class SocketStream;

typedef std::function<void (const std::shared_ptr<Connection>)> ConnectionCallback;
typedef std::function<void (const std::shared_ptr<Connection>)> CloseCallback;
typedef std::function<void (const std::shared_ptr<Connection>)> WriteCompleteCallback;
typedef std::function<void (const std::shared_ptr<Connection>, size_t)> HighWaterMarkCallback;
typedef std::function<void (const std::shared_ptr<Connection>, uint64_t)> MessageCallback;

class Connection : public std::enable_shared_from_this<Connection>, Noncopyable {
public:
    typedef std::shared_ptr<Connection> ptr;
    typedef Mutex MutexType;
    Connection(EventLoop* loop,
               const std::string& name,
               const std::shared_ptr<Socket> socket, 
               const std::shared_ptr<SocketStream> stream);
    ~Connection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& getName() const { return name_; }
    std::shared_ptr<Socket> getSocket() const { return socket_; }
    bool isConnected() const { return state_ == kConnected; }
    bool isDisconnected() const { return state_ == kDisconnected; }
    bool isConnecting() const { return state_ == kConnecting; }
   
    void send(const void* message, int len);
    void send(const std::string& message);
    void send(std::shared_ptr<Buffer> message);  
    void shutdown(); 
    void forceClose();
    void forceCloseWithDelay(uint64_t seconds);
 
    void startRead();
    void stopRead();

    bool isReading() const;

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { 
        highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; 
    }
    
    std::shared_ptr<SocketStream> getStream() const { return stream_; }

    std::shared_ptr<Buffer> inputBuffer() const { return inputBuffer_; }

    std::shared_ptr<Buffer> outputBuffer() const { return outputBuffer_; }

    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

    void connectEstablished(); 
    void connectDestroyed();  
private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void handleRead(uint64_t receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const std::string& message);
    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    void forceCloseInLoop();
    void setState(StateE s) { state_ = s; }
    const char* stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();
   
    EventLoop* loop_;
    const std::string name_;
    std::shared_ptr<Socket> socket_;
    std::shared_ptr<SocketStream> stream_;
    std::atomic<StateE> state_; 
    bool reading_;
    std::unique_ptr<Channel> channel_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;
    std::shared_ptr<Buffer> inputBuffer_;
    std::shared_ptr<Buffer> outputBuffer_; 
};
}
#endif