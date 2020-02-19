#include "eventloop.h"
#include "timerqueue.h"
#include "channel.h"
#include "poller.h"
#include "log.h"
#include "macro.h"
#include <fcntl.h>
#include <sys/timerfd.h>

namespace fylee {

static fylee::Logger::ptr g_logger = LOG_NAME("system");
static thread_local EventLoop* t_loopInThisThread = nullptr;
static const int kPollTimeMs = 10000;

EventLoop* EventLoop::GetEventLoopOfCurrentThread() {
  return t_loopInThisThread;
}

EventLoop::EventLoop() 
    :looping_(false),
     quit_(false),
     eventHandling_(false),
     callingPendingFunctors_(false),
     iteration_(0),
     threadId_(fylee::GetThreadId()),
     poller_(new Poller(this)),
     timerQueue_(new TimerQueue(this)),
     currentActiveChannel_(nullptr) {
   
    int rt = pipe(wakeupFds_);
    ASSERT(!rt);
    rt = fcntl(wakeupFds_[0], F_SETFL, O_NONBLOCK);
    ASSERT(!rt);
    wakeupChannel_.reset(new Channel(this, wakeupFds_[0]));

    LOG_DEBUG(g_logger) << "EventLoop created " << this << " in thread " << threadId_;
    if (t_loopInThisThread) {
        LOG_FATAL(g_logger) << "Another EventLoop " << t_loopInThisThread 
                            << " exists in this thread " << threadId_;
    } else {
        t_loopInThisThread = this;
    }
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // we are always reading the wakeupfd
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    LOG_DEBUG(g_logger) << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << fylee::GetThreadId();
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    close(wakeupFds_[0]);
    close(wakeupFds_[1]);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    ASSERT(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
    LOG_INFO(g_logger) << "EventLoop " << this << " start looping";

    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        ++iteration_;
        if (g_logger->getLevel() <= LogLevel::INFO) {
            printActiveChannels();
        }
     
        eventHandling_ = true;
        for (auto channel : activeChannels_) {
            currentActiveChannel_ = channel;
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = nullptr;
        eventHandling_ = false;
        doPendingFunctors();
    }

    LOG_INFO(g_logger) << "EventLoop " << this << " stop looping";
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
   
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        MutexType::Lock lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

size_t EventLoop::queueSize() const {
    MutexType::Lock lock(mutex_);
    return pendingFunctors_.size();
}

Timer::ptr EventLoop::runAt(uint64_t time, Functor cb) {
    uint64_t now_ms = fylee::GetCurrentMS();
    if(time >= now_ms) {
        return runAfter(time - now_ms, cb);
    } else {
        return nullptr;
    }
}

Timer::ptr EventLoop::runAfter(uint64_t delay, Functor cb) {
    assertInLoopThread();
    return timerQueue_->addTimer(delay, cb, false);
}

Timer::ptr EventLoop::runEvery(uint64_t interval, Functor cb) {
    return timerQueue_->addTimer(interval, cb, true);
}

void EventLoop::cancel(Timer::ptr timer) {
    timerQueue_->cancelTimer(timer);
}

void EventLoop::updateChannel(Channel* channel) {
    ASSERT(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    ASSERT(channel->ownerLoop() == this);
    assertInLoopThread();
    if(eventHandling_) {
        ASSERT(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
    }
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
    ASSERT(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread() {
    LOG_FATAL(g_logger) << "EventLoop::abortNotInLoopThread - EventLoop " << this 
                << " was created in threadId_ = " << threadId_
                << ", current thread id = " <<  fylee::GetThreadId();
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFds_[1], &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR(g_logger) << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::handleRead() {
    LOG_DEBUG(g_logger) << "EventLoop::handleRead()";
    uint64_t one = 1;
    ssize_t n = read(wakeupFds_[0], &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR(g_logger) << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        MutexType::Lock lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (auto& functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const {
    for (auto channel : activeChannels_) {
        LOG_INFO(g_logger) << "{" << channel->reventsToString() << "} ";
    }
}
}