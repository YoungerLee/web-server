#ifndef __FYLEE_EVENTLOOPTHREADPOOL_H_
#define __FYLEE_EVENTLOOPTHREADPOOL_H_
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include "noncopyable.h"

namespace fylee {
class EventLoop;
class EventLoopThread;
class EventLoopThreadPool : Noncopyable {
public:
    typedef std::shared_ptr<EventLoopThreadPool> ptr;
    typedef std::function<void(EventLoop*)> ThreadInitCallback;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& name);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // valid after calling start()
    // round-robin
    EventLoop* getNextLoop();

    /// with the same hash code, it will always return the same EventLoop
    EventLoop* getLoopForHash(size_t hashCode);

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }

    const std::string& name() const { return name_; }

private:
    EventLoop* baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // 线程池
    std::vector<EventLoop*> loops_;
};
}
#endif