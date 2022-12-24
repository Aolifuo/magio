#ifndef MAGIO_CORE_CO_CONTEXT_H_
#define MAGIO_CORE_CO_CONTEXT_H_

#include <deque>
#include <mutex>

#include "magio-v3/core/coro.h"
#include "magio-v3/core/timer_queue.h"

namespace magio {

class CoroContext: Noncopyable, public Executor {
public:
    enum State {
        Running, Sleeping, Stopping, 
    };

    CoroContext();

    CoroContext(std::unique_ptr<IoService> service);

    void start() override;

    void stop() override;

    void execute(Task&& task) override;

    template<typename Rep, typename Per>
    TimerHandle expires_after(const std::chrono::duration<Rep, Per>& dur, TimerTask&& task) {
        return timer_queue_.push(TimerClock::now() + dur, std::move(task));
    }

    TimerHandle expires_until(const TimerClock::time_point& tp, TimerTask&& task) {
        return timer_queue_.push(tp, std::move(task));
    }

    void wake_in_context(std::coroutine_handle<>);

    IoService& get_service() const;

private:
    void handle_io_poller();

    void wake_up();

    bool assert_in_context_thread();

    std::mutex mutex_;

    State state_ = Stopping;
    unsigned thread_id_;
    std::vector<std::coroutine_handle<>> pending_handles_;
    TimerQueue timer_queue_;
    std::unique_ptr<IoService> p_io_service_;
};

}

#include "magio-v3/core/impl/coro.hpp"
#include "magio-v3/core/impl/this_context.hpp"

#endif