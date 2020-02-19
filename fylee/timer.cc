#include "timer.h"
#include "util.h"
#include "weakcb.h"

namespace fylee {

bool Timer::Comparator::operator()(const Timer::ptr& lhs
                        ,const Timer::ptr& rhs) const {
    if(!lhs && !rhs) {
        return false;
    }
    if(!lhs) {
        return true;
    }
    if(!rhs) {
        return false;
    }
    if(lhs->next_ < rhs->next_) {
        return true;
    }
    if(rhs->next_ < lhs->next_) {
        return false;
    }
    return lhs.get() < rhs.get();
}


Timer::Timer(uint64_t ms, Functor func, bool recurring)
    :recurring_(recurring),
     ms_(ms),
     func_(std::move(func)) {
    next_ = fylee::GetCurrentMS() + ms_;
}

Timer::Timer(uint64_t next)
    :next_(next) {
}

}
