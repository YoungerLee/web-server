#include "timerqueue.h"
#include "log.h"
#include "util.h"
#include "eventloop.h"
#include "channel.h"
#include "macro.h"
#include <sys/timerfd.h>

namespace fylee {
static fylee::Logger::ptr g_logger = LOG_NAME("system");
TimerQueue::TimerQueue(EventLoop* loop) 
    :loop_(loop),
     timerfd_(createTimerfd()),
     timerfdChannel_(new Channel(loop, timerfd_)),
     callingExpiredTimers_(false) {
    previouseTime_ = fylee::GetCurrentMS();
    timerfdChannel_->setReadCallback(std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_->enableReading();
}

TimerQueue::~TimerQueue() {
    timerfdChannel_->disableAll();
    timerfdChannel_->remove();
    close(timerfd_);
}

Timer::ptr TimerQueue::addTimer(uint64_t ms, Functor func, bool recurring) {
    Timer::ptr timer(new Timer(ms, func, recurring));
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return timer;
}

void TimerQueue::cancelTimer(Timer::ptr timer) {
    loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timer));
}

void TimerQueue::addTimerInLoop(Timer::ptr timer) {
    RWMutexType::WriteLock lock(mutex_);
    auto it = timers_.insert(timer).first;
    bool at_front = (it == timers_.begin()) && !tickled_;
    if(at_front) {
        tickled_ = true;
    }
    lock.unlock();

    if(at_front) {
        onTimerInsertedAtFront(timer->getExpiration());
    }
}

void TimerQueue::cancelInLoop(Timer::ptr timer) {
    loop_->assertInLoopThread();
    RWMutexType::WriteLock lock(mutex_);
    bool rt = false;
    if(timer->func_) {
        timer->func_ = nullptr;
        auto it = timers_.find(timer);
        timers_.erase(it);
        rt = true;
    }
    ASSERT(rt);
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void ()> func) {
    if(weak_cond.lock()) {
        func();
    }
}

Timer::ptr TimerQueue::addConditionTimer(uint64_t ms, Functor func
                                    ,std::weak_ptr<void> weak_cond
                                    ,bool recurring) {
    return addTimer(ms, std::bind(&OnTimer, weak_cond, func), recurring);
}

uint64_t TimerQueue::getNextTimer() {
    RWMutexType::ReadLock lock(mutex_);
    tickled_ = false;
    if(timers_.empty()) { // 返回0ull表示没有定时器事件了
        return ~0ull;
    }

    const Timer::ptr& next = *timers_.begin();
    uint64_t now_ms = fylee::GetCurrentMS();
    if(now_ms >= next->next_) {
        return 0;
    } else {
        return next->next_ - now_ms;
    }
}

uint64_t TimerQueue::getFrontTimer() {
    RWMutexType::ReadLock lock(mutex_);
    tickled_ = false;
    if(timers_.empty()) { // 返回0ull表示没有定时器事件了
        return ~0ull;
    }

    const Timer::ptr& next = *timers_.begin();
    return next->next_;
}

void TimerQueue::listExpiredFunc(std::vector<Functor>& funcs) {
    uint64_t now_ms = fylee::GetCurrentMS();
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock lock(mutex_);
        if(timers_.empty()) {
            return;
        }
    }
    RWMutexType::WriteLock lock(mutex_);
    if(timers_.empty()) {
        return;
    }
    bool rollover = detectClockRollover(now_ms); // 检测是否超时仍未执行
    if(!rollover && ((*timers_.begin())->next_ > now_ms)) {
        return;
    }

    Timer::ptr now_timer(new Timer(now_ms));
    auto it = rollover ? timers_.end() : timers_.lower_bound(now_timer);// 找出第一个大于等于now_ms的定时器
    while(it != timers_.end() && (*it)->next_ == now_ms) {
        ++it;
    }
    expired.insert(expired.begin(), timers_.begin(), it); // 将所有超时仍未执行的回调加入expired
    timers_.erase(timers_.begin(), it);
    funcs.reserve(expired.size());

    for(auto& timer : expired) {
        funcs.push_back(timer->func_);
        if(timer->recurring_) {
            timer->next_ = now_ms + timer->ms_;
            timers_.insert(timer);
        } else {
            timer->func_ = nullptr;
        }
    }
}

bool TimerQueue::detectClockRollover(uint64_t now_ms) {
    bool rollover = false;
    if(now_ms < previouseTime_ &&
            now_ms < (previouseTime_ - 60 * 60 * 1000)) {
        rollover = true;
    }
    previouseTime_ = now_ms;
    return rollover;
}

bool TimerQueue::hasTimer() {
    RWMutexType::ReadLock lock(mutex_);
    return !timers_.empty();
}

int TimerQueue::createTimerfd() {
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                    TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG_FATAL(g_logger) << "Failed in timerfd_create";
    }
    return timerfd;
}

struct timespec TimerQueue::howMuchTimeFromNow(uint64_t when_ms) {
    static const uint64_t uSecPerSec = 1000 * 1000;
    uint64_t now_us = fylee::GetCurrentUS();
    uint64_t from_now_us = when_ms * 1000ul - now_us;
    if(from_now_us < 100) {
        from_now_us = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(from_now_us / uSecPerSec);
    ts.tv_nsec = static_cast<long>((from_now_us % uSecPerSec) * 1000ul);
    return ts;
}

void TimerQueue::readTimerfd(int timerfd, uint64_t now) {
    uint64_t howmany;
    ssize_t n = read(timerfd, &howmany, sizeof(howmany));
    LOG_INFO(g_logger) << "TimerQueue::handleRead() " << howmany << " at " << std::to_string(now);
    if (n != sizeof(howmany)) {
        LOG_ERROR(g_logger) << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

void TimerQueue::resetTimerfd(int timerfd, uint64_t expiration) {
    struct itimerspec newValue;
    struct itimerspec oldValue;
 
    bzero(&newValue, sizeof(newValue));
    bzero(&oldValue, sizeof(oldValue));
    newValue.it_value = howMuchTimeFromNow(expiration);
    int rt = timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (rt) {
        LOG_ERROR(g_logger) << "timerfd_settime() failed";
    }
}

void TimerQueue::handleRead() {
    loop_->assertInLoopThread();
    std::vector<Functor> expired;
    listExpiredFunc(expired);
  
    uint64_t now_ms = fylee::GetCurrentMS();
    readTimerfd(timerfd_, now_ms);

    callingExpiredTimers_ = true;
    for (auto& it : expired) {
        it();
    }
    callingExpiredTimers_ = false;
    uint64_t nextExpire = getFrontTimer();
    if(nextExpire < ~0ull) {
        resetTimerfd(timerfd_, nextExpire);
    }
}

void TimerQueue::onTimerInsertedAtFront(uint64_t earliest) {
    resetTimerfd(timerfd_, earliest);
}

}