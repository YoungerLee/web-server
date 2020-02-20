#ifndef __FYLEE_CHANNEL_H_
#define __FYLEE_CHANNEL_H_
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <functional>

#include "noncopyable.h"

namespace fylee {
class EventLoop;
class Channel : Noncopyable, std::enable_shared_from_this<Channel> {
public:
    typedef std::shared_ptr<Channel> ptr;
    typedef std::function<void()> EventCallback;
    typedef std::function<void(uint64_t)> ReadEventCallback;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent(uint64_t receiveTime);
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }
  
    void tie(const std::shared_ptr<void>&);

    int getFd() const { return fd_; }
    int getEvents() const { return events_; }
    void setRevents(int revt) { revents_ = revt; } 
  
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }
    bool isWriting() const { return events_ & EPOLLOUT; }
    bool isReading() const { return events_ & EPOLLIN; }

    // for Poller
    int getIndex() { return index_; }
    void setIndex(int idx) { index_ = idx; }

    // for debug
    std::string reventsToString() const;
    std::string eventsToString() const;

    void doNotLogHup() { logHup_ = false; }

    EventLoop* ownerLoop() { return loop_; }
    void remove();

 private:
    enum Event {
        kNoneEvent = 0,
        kReadEvent = EPOLLIN | EPOLLET,
        kWriteEvent = EPOLLOUT,
    };
    static std::string eventsToString(int fd, EPOLL_EVENTS event);

    void update();
    void handleEventWithGuard(uint64_t receiveTime);

    EventLoop* loop_;
    const int  fd_;
    int        events_;
    int        revents_; 
    int        index_; 
    bool       logHup_;

    std::weak_ptr<void> tie_;
    bool tied_;
    bool eventHandling_;
    bool addedToLoop_;
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
}
#endif