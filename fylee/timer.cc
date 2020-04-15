#include "timer.h"
#include "util.h"

namespace fylee {

bool Timer::Comparator::operator()(const Timer::ptr& lhs
                        ,const Timer::ptr& rhs) const {
    return lhs->next_ > rhs->next_;
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
