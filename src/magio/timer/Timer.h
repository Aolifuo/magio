#pragma once

#include <cstdio>
#include "magio/core/Queue.h"
#include "magio/execution/Execution.h"
#include "magio/coro/UseCoro.h"

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

    void async_wait(CompletionHandler handler) {
        timer_ids_.push(executor_.set_timeout(timeout_, std::move(handler)));
    }

    Coro<void> async_wait(UseCoro) {
        return {
            [=](std::coroutine_handle<> h) {
                executor_.set_timeout(timeout_, [=] {
                    if (h) {
                        h.resume();
                    }
                });
            }
        };
    }

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

    AnyExecutor get_executor() {
        return executor_;
    }
private:
    static void handle_resume(AnyExecutor executor, size_t ms, std::coroutine_handle<> h) {
        if (h) {
            h.resume();
        } else {
            executor.set_timeout(ms, [=] { handle_resume(executor, ms, h); });
        }
    }

    AnyExecutor executor_;
    size_t timeout_;
    RingQueue<TimerID> timer_ids_;
};

} // namespace magio