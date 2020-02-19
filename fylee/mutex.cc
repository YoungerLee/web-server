#include "mutex.h"
#include "macro.h"

namespace fylee {

Semaphore::Semaphore(uint32_t count) {
    if(sem_init(&semaphore_, 0, count)) {
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&semaphore_);
}

void Semaphore::wait() {
    if(sem_wait(&semaphore_)) {
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify() {
    if(sem_post(&semaphore_)) {
        throw std::logic_error("sem_post error");
    }
}

Mutex::Mutex() : locked_(false) {
    pthread_mutex_init(&mutex_, nullptr);
}

Mutex::~Mutex() {
    ASSERT(!locked_);
    pthread_mutex_destroy(&mutex_);
}

void Mutex::lock() {
    pthread_mutex_lock(&mutex_);
    locked_ = true;    
}

void Mutex::unlock() {
    locked_ = false;
    pthread_mutex_unlock(&mutex_);
}

Condition::Condition(Mutex& mutex)
:mutex_(mutex) {
    pthread_cond_init(&pcond_, nullptr);
}

Condition::~Condition() {
    pthread_cond_destroy(&pcond_);
}

void Condition::wait() {
    assert(mutex_.isLocked());
    pthread_cond_wait(&pcond_, mutex_.getPtr());
    mutex_.restoreMutexStatus();
}

void Condition::notify() {
    pthread_cond_signal(&pcond_);
}

void Condition::notifyAll() {
    pthread_cond_broadcast(&pcond_);
}

bool Condition::waitForSeconds(double seconds)
{
    struct timespec abstime;
    // FIXME: use CLOCK_MONOTONIC or CLOCK_MONOTONIC_RAW to prevent time rewind.
    clock_gettime(CLOCK_REALTIME, &abstime);

    const int64_t kNanoSecondsPerSecond = 1000000000;
    int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);

    abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) / kNanoSecondsPerSecond);
    abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) % kNanoSecondsPerSecond);

    ASSERT(!mutex_.isLocked());
    return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.getPtr(), &abstime);
}

CountDownLatch::CountDownLatch(int count)
:mutex_(),
condition_(mutex_),
count_(count) { }

void CountDownLatch::wait() {
    Mutex::Lock lock(mutex_);
    while (count_ > 0) 
        condition_.wait();
}

void CountDownLatch::countDown() {
    Mutex::Lock lock(mutex_);
    --count_;
    if (count_ == 0) 
        condition_.notifyAll();
}

int CountDownLatch::getCount() const {
    Mutex::Lock lock(mutex_);
    return count_;
}

SpinLock::SpinLock() : locked_(false) {
    pthread_spin_init(&mutex_, 0);
}

SpinLock::~SpinLock() {
    ASSERT(!locked_);
    pthread_spin_destroy(&mutex_);
}

void SpinLock::lock() {
    pthread_spin_lock(&mutex_);
    locked_ = true;   
}

void SpinLock::unlock() {
    locked_ = false;
    pthread_spin_unlock(&mutex_);
}

}
