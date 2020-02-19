#include "eventloop.h"
#include "eventloopthread.h"
#include "eventloopthreadpool.h"
#include "macro.h"

namespace fylee {

template<typename To, typename From>
inline To implicit_cast(From const &f) {
    return f;
}

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& name) 
    :baseLoop_(baseLoop),
     name_(name), 
     started_(false), 
     numThreads_(0), 
     next_(0) { }

EventLoopThreadPool::~EventLoopThreadPool() {
  // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    ASSERT(!started_);
    baseLoop_->assertInLoopThread();

    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());
    }
    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    baseLoop_->assertInLoopThread();
    ASSERT(started_);
    EventLoop* loop = baseLoop_;

    if (!loops_.empty()) {
        // round-robin
        loop = loops_[next_];
        ++next_;
        if (implicit_cast<size_t>(next_) >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode) {
    baseLoop_->assertInLoopThread();
    EventLoop* loop = baseLoop_;

    if (!loops_.empty()) {
        loop = loops_[hashCode % loops_.size()];
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    baseLoop_->assertInLoopThread();
    ASSERT(started_);
    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseLoop_);
    } else {
        return loops_;
    }
}
}