#include "thread.h"
#include "log.h"
#include "util.h"
#include "macro.h"

namespace fylee {

static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";

static fylee::Logger::ptr g_logger = LOG_NAME("system");

Thread* Thread::GetThis() {
    return t_thread;
}

const std::string& Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string& name) {
    if(name.empty()) {
        return;
    }
    if(t_thread) {
        t_thread->name_ = name;
    }
    t_thread_name = name;
}

Thread::Thread(Functor func, const std::string& name)
    :func_(std::move(func)),
     started_(false),
     joined_(false), 
     name_(name), 
     latch_(1) {
    if(name.empty()) {
        name_ = "UNKNOW";
    }  
}

Thread::~Thread() {
    if(started_ && !joined_) {
        pthread_detach(thread_);
    }
}

void Thread::start() {
    ASSERT(!started_);
    started_ = true;
    int rt = pthread_create(&thread_, nullptr, &Thread::run, this);
    if(rt) {
        started_ = false;
        LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
            << " name=" << name_;
        throw std::logic_error("pthread_create error");
    }
    // semaphore_.wait();
    latch_.wait();
}

void Thread::join() {
    ASSERT(started_);
    ASSERT(!joined_);
    joined_ = true;
    if(thread_) {
        int rt = pthread_join(thread_, nullptr);
        if(rt) {
            joined_ = false;
            LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt
                << " name=" << name_;
            throw std::logic_error("pthread_join error");
        }
        thread_ = 0;
    }
}

void* Thread::run(void* arg) {
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    t_thread_name = thread->name_;
    thread->tid_ = fylee::GetThreadId();
    pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).c_str());

    Functor func;
    func.swap(thread->func_);

    // thread->semaphore_.notify();
    thread->latch_.countDown();
    func();
    return 0;
}

}
