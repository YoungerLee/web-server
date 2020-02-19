#ifndef __FYLEE_MUTEX_H__
#define __FYLEE_MUTEX_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>
#include "noncopyable.h"

namespace fylee {

class Semaphore : Noncopyable {
public:
    Semaphore(uint32_t count = 0);

    ~Semaphore();

    void wait();

    void notify();
private:
    sem_t semaphore_;
};

template<class T>
struct ScopedLockImpl {
public:
    ScopedLockImpl(T& mutex)
        :mutex_(mutex) {
        mutex_.lock();
    }

    ~ScopedLockImpl() {
        unlock();
    }

    void lock() {
        mutex_.lock();
    }

    void unlock() {
        mutex_.unlock();
    }

    bool isLocked() const { return mutex_.isLocked_(); }

private:
    T& mutex_;
};

template<class T>
struct ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T& mutex)
        :mutex_(mutex) {
        mutex_.rdlock();
        locked_ = true;
    }

    ~ReadScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!locked_) {
            mutex_.rdlock();
            locked_ = true;
        }
    }

    void unlock() {
        if(locked_) {
            mutex_.unlock();
            locked_ = false;
        }
    }
private:
    T& mutex_;
    bool locked_;
};

template<class T>
struct WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T& mutex)
        :mutex_(mutex) {
        mutex_.wrlock();
        locked_ = true;
    }

    ~WriteScopedLockImpl() {
        unlock();
    }

    void lock() {
        if(!locked_) {
            mutex_.wrlock();
            locked_ = true;
        }
    }

    void unlock() {
        if(locked_) {
            mutex_.unlock();
            locked_ = false;
        }
    }
private:
    T& mutex_;
    bool locked_;
};

class Mutex : Noncopyable {
friend class Condition;
public: 
    typedef ScopedLockImpl<Mutex> Lock;

    Mutex();

    ~Mutex();

    void lock();

    void unlock();

    bool isLocked() const { return locked_; }

    
private:
    pthread_mutex_t mutex_;
    bool locked_;
    pthread_mutex_t* getPtr() { return &mutex_; } // for condition

    void restoreMutexStatus() { locked_ = true; } // for condition
    
};

class NullMutex : Noncopyable{
public:
    typedef ScopedLockImpl<NullMutex> Lock;

    NullMutex() : locked_(false) {}

    ~NullMutex() { unlock(); }

    void lock() { locked_= true; }

    void unlock() { locked_ = false; }

    bool isLocked() const { return locked_; }
private:
    bool locked_;
};

class Condition : Noncopyable
{
 public:
    explicit Condition(Mutex& mutex);

    ~Condition();

    void wait();

    // returns true if time out, false otherwise.
    bool waitForSeconds(double seconds);

    void notify();

    void notifyAll();

private:
    Mutex& mutex_;
    pthread_cond_t pcond_;
};

class CountDownLatch : Noncopyable {
public:
    explicit CountDownLatch(int count);

    void wait();

    void countDown();

    int getCount() const;

private:
    mutable Mutex mutex_;
    Condition condition_;
    int count_;
};

class RWMutex : Noncopyable{
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    RWMutex() {
        pthread_rwlock_init(&lock_, nullptr);
    }

    ~RWMutex() {
        pthread_rwlock_destroy(&lock_);
    }

    void rdlock() {
        pthread_rwlock_rdlock(&lock_);
    }

    void wrlock() {
        pthread_rwlock_wrlock(&lock_);
    }

    void unlock() {
        pthread_rwlock_unlock(&lock_);
    }
private:
    pthread_rwlock_t lock_;
};

class NullRWMutex : Noncopyable {
public:
    typedef ReadScopedLockImpl<NullMutex> ReadLock;
    typedef WriteScopedLockImpl<NullMutex> WriteLock;

    NullRWMutex() {}

    ~NullRWMutex() {}

    void rdlock() {}

    void wrlock() {}

    void unlock() {}
};


class SpinLock : Noncopyable {
public:
    typedef ScopedLockImpl<SpinLock> Lock;

    SpinLock();

    ~SpinLock();

    void lock();

    void unlock();

    bool isLocked() const { return locked_; }
private:
    pthread_spinlock_t mutex_;
    bool locked_;
};

class CASLock : Noncopyable {
public:
    typedef ScopedLockImpl<CASLock> Lock;

    CASLock() : locked_(false) {
        mutex_.clear();
    }

    ~CASLock() {
    }

    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&mutex_, std::memory_order_acquire));
        locked_ = true;  
    }

    void unlock() {
        locked_ = false;
        std::atomic_flag_clear_explicit(&mutex_, std::memory_order_release);  
    }
private:
    volatile std::atomic_flag mutex_;
    bool locked_; 
};

}

#endif
