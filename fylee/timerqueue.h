#ifndef __FYLEE_TIMERQUEUE_H__
#define __FYLEE_TIMERQUEUE_H__

#include <functional>
#include "timer.h"
#include "noncopyable.h"

namespace fylee {
class EventLoop;
class Channel;
/**
 * @brief 定时器管理器
 */
class TimerQueue : public Noncopyable {
friend class Timer;
public:
    typedef std::function<void()> Functor;
  
    typedef RWMutex RWMutexType;

    TimerQueue(EventLoop* loop);

    ~TimerQueue();

    Timer::ptr addTimer(uint64_t ms, Functor func, bool recurring = false);
    
    void cancelTimer(Timer::ptr timer);


    Timer::ptr addConditionTimer(uint64_t ms, Functor func,
                                 std::weak_ptr<void> weak_cond, 
                                 bool recurring = false);

    uint64_t getNextTimer();

    uint64_t getFrontTimer();

    void listExpiredFunc(std::vector<Functor >& funcs);

    bool hasTimer();

    void onTimerInsertedAtFront(uint64_t earliest);

private:
    // Mutex
    RWMutexType mutex_;
    // 定时器集合
    std::set<Timer::ptr, Timer::Comparator> timers_;
    // std::priority_queue<Timer::ptr, std::vector<Timer::ptr>, Timer::Comparator> timers_;
    // 是否触发onTimerInsertedAtFront
    bool tickled_ = false;
    // 上次执行时间
    uint64_t previouseTime_ = 0;

    /**
     * @brief 检测服务器时间是否被调后了
     */
    bool detectClockRollover(uint64_t now_ms);

    int createTimerfd();
    struct timespec howMuchTimeFromNow(uint64_t when_ms);
    void readTimerfd(int timerfd, uint64_t now);
    void resetTimerfd(int timerfd, uint64_t expiration);
    void addTimerInLoop(Timer::ptr timer);
    void cancelInLoop(Timer::ptr timer);
    // called when timerfd alarms
    void handleRead();

    EventLoop* loop_;
    const int timerfd_;
    std::unique_ptr<Channel> timerfdChannel_;
    bool callingExpiredTimers_; /* atomic */
};
}
#endif