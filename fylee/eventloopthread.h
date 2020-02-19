#ifndef __FYLEE_EVENTLOOPTHREAD_H_
#define __FYLEE_EVENTLOOPTHREAD_H_
#include <atomic>
#include <functional>
#include <memory>

#include "thread.h"
#include "util.h"

namespace fylee {
class EventLoop;
class EventLoopThread : Noncopyable {
public:
    typedef std::shared_ptr<EventLoopThread> ptr;
    typedef std::function<void(EventLoop*)> ThreadInitCallback;
    typedef Mutex MutexType;
    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                    const std::string& name = std::string());
    ~EventLoopThread();
    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    MutexType mutex_;
    Condition cond_;
    ThreadInitCallback callback_;
};
}
#endif
