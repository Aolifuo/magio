#pragma once

#include <cstdio>
#include "magio/core/Queue.h"
#include "magio/coro/Fwd.h"
#include "magio/execution/Execution.h"

namespace magio {

class Timer {
public:
    Timer(AnyExecutor executor, size_t ms = 0)
        : executor_(executor), timeout_(ms)
    { }

    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

    ~Timer() {
        cancel_all();
    }

    void expire_after(size_t ms) {
        timeout_ = ms;
    }

    void async_wait(CompletionHandler&& handler) {
        timer_ids_.push(executor_.set_timeout(timeout_, std::move(handler)));
    }

    Coro<void> async_wait(detail::UseCoro);

    void cancel_one() {
        executor_.clear(timer_ids_.front());
        timer_ids_.pop();
    }

    void cancel_all() {
        for (; !timer_ids_.empty();) {
            executor_.clear(timer_ids_.front());
            timer_ids_.pop();
        }
    }

    AnyExecutor get_executor() const {
        return executor_;
    }
private:
    AnyExecutor executor_;
    size_t timeout_;
    RingQueue<TimerID> timer_ids_;
};

} // namespace magio