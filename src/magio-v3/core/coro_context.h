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
        Running, Stopping, 
    };

    CoroContext(size_t entries);

    void start();

    void stop();

    void execute(Task&& task) override;

    void dispatch(Task&& task);

#ifdef MAGIO_USE_CORO
    template<typename T>
    void spawn(Coro<T> coro) {
        queue_in_context(coro.handle());
    }

    template<typename T>
    void spawn(Coro<T> coro, CoroCompletionHandler<T>&& handler) {
        coro.set_callback(std::move(handler));
        queue_in_context(coro.handle());
    }

    void wake_in_context(std::coroutine_handle<>);

    void queue_in_context(std::coroutine_handle<>);
#endif

    template<typename Rep, typename Per>
    TimerHandle expires_after(const std::chrono::duration<Rep, Per>& dur, TimerTask&& task) {
        return timer_queue_.push(TimerClock::now() + dur, std::move(task));
    }

    TimerHandle expires_until(const TimerClock::time_point& tp, TimerTask&& task) {
        return timer_queue_.push(tp, std::move(task));
    }

    bool assert_in_context_thread();

    IoService& get_service() const;

private:
    void wake_up();

    void handle_io_poller();

    std::mutex mutex_;

    State state_ = Stopping;
    size_t thread_id_;
#ifdef MAGIO_USE_CORO
    std::vector<std::coroutine_handle<>> pending_handles_;
#else
    std::vector<Task> pending_handles_;
#endif
    TimerQueue timer_queue_;
    std::unique_ptr<IoService> p_io_service_;
};

}

#include "magio-v3/core/impl/coro.hpp"
#include "magio-v3/core/impl/this_context.hpp"

#endif