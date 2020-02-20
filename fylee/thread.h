#ifndef __FYLEE_THREAD_H__
#define __FYLEE_THREAD_H__

#include "mutex.h"

namespace fylee {

class Thread : Noncopyable {
public:
    typedef std::shared_ptr<Thread> ptr;
    typedef std::function<void()> Functor;
    Thread(Functor func, const std::string& name);

    ~Thread();

    pid_t getTid() const { return tid_;}

    bool isStarted() const { return started_; }

    const std::string& getName() const { return name_;}

    void start();

    void join();

    static Thread* GetThis();

    static const std::string& GetName();

    static void SetName(const std::string& name);
private:
    static void* run(void* arg);
    pid_t tid_ = -1;
    pthread_t thread_ = 0;
    Functor func_;
    bool started_;
    bool joined_;
    std::string name_;
    CountDownLatch latch_;
};

}

#endif
