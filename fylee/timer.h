#ifndef __FYLEE_TIMER_H__
#define __FYLEE_TIMER_H__

#include <memory>
#include <vector>
#include <set>
#include <functional>
#include "thread.h"

namespace fylee {
class TimerQueue;
/**
 * @brief 定时器
 */
class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerQueue;
public:
    typedef std::function<void()> Functor;
    /// 定时器的智能指针类型
    typedef std::shared_ptr<Timer> ptr;

    uint64_t getExpiration() { return next_; }

    /**
     * @brief 构造函数
     * @param[in] ms 定时器执行间隔时间
     * @param[in] func 回调函数
     * @param[in] recurring 是否循环
     * @param[in] manager 定时器管理器
     */
    Timer(uint64_t ms, Functor func, bool recurring);
    /**
     * @brief 构造函数
     * @param[in] next 执行的时间戳(毫秒)
     */
    Timer(uint64_t next);
private:
    /// 是否循环定时器
    bool recurring_ = false;
    /// 执行周期
    uint64_t ms_ = 0;
    /// 精确的执行时间
    uint64_t next_ = 0;
    /// 回调函数
    Functor func_;
    /**
     * @brief 定时器比较仿函数
     */
    struct Comparator {
        /**
         * @brief 比较定时器的智能指针的大小(按执行时间排序)
         * @param[in] lhs 定时器智能指针
         * @param[in] rhs 定时器智能指针
         */
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };
};

}

#endif