#ifndef __FYLEE_EVENTLOOP_H_
#define __FYLEE_EVENTLOOP_H_

#include <atomic>
#include <functional>
#include <memory>
#include "thread.h"
#include "timer.h"
#include "util.h"

namespace fylee {

class Channel;
class Poller;
class TimerQueue;

class EventLoop : Noncopyable {
public:
    typedef std::function<void()> Functor;
    typedef std::shared_ptr<EventLoop> ptr;
    typedef Mutex MutexType;
    EventLoop();
    ~EventLoop(); 

    void loop();

    void quit();

    uint64_t pollReturnTime() const { return pollReturnTime_; }

    int64_t iteration() const { return iteration_; }

    void runInLoop(Functor cb);

    void queueInLoop(Functor cb);

    size_t queueSize() const;

    Timer::ptr runAt(uint64_t time, Functor cb);
   
    Timer::ptr runAfter(uint64_t delay, Functor cb);
   
    Timer::ptr runEvery(uint64_t interval, Functor cb);
 
    void cancel(Timer::ptr timer);

    void wakeup();
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    void assertInLoopThread() {
        if (!isInLoopThread()) {
            abortNotInLoopThread();
        }
    }
    bool isInLoopThread() const { return threadId_ == fylee::GetThreadId(); }

    bool eventHandling() const { return eventHandling_; }

    static EventLoop* GetEventLoopOfCurrentThread();

private:
    void abortNotInLoopThread();

    void handleRead();  // waked up

    void doPendingFunctors();

    void printActiveChannels() const; // DEBUG

    typedef std::vector<Channel*> ChannelList;

    bool looping_; /* atomic */
    std::atomic<bool> quit_;
    bool eventHandling_; /* atomic */
    bool callingPendingFunctors_; /* atomic */
    int64_t iteration_;
    const pid_t threadId_;
    uint64_t pollReturnTime_;
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    int wakefd_;
    std::unique_ptr<Channel> wakeupChannel_;

    // scratch variables
    ChannelList activeChannels_;
    Channel* currentActiveChannel_;
    mutable MutexType mutex_;
    std::vector<Functor> pendingFunctors_;

};
} 
#endif