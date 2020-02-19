#include "eventloopthread.h"
#include "eventloop.h"
#include "macro.h"

namespace fylee {
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name) 
    :loop_(nullptr),
     exiting_(false),
     thread_(std::bind(&EventLoopThread::threadFunc, this), name),
     mutex_(),
     cond_(mutex_),
     callback_(std::move(cb)) { }

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    ASSERT(!thread_.isStarted());
    thread_.start();

    EventLoop* loop = nullptr;
    {
        MutexType::Lock lock(mutex_);
        while (!loop_) {
            cond_.wait();
        }
        loop = loop_;
    }

    return loop;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;

    if (callback_) {
        callback_(&loop);
    }

    {
        MutexType::Lock lock(mutex_);
        loop_ = &loop;
        cond_.notify();
    }
    loop.loop();

    MutexType::Lock lock(mutex_);
    loop_ = nullptr;
}

}