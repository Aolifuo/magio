#ifndef MAGIO_CORE_THREAD_POOL_H_
#define MAGIO_CORE_THREAD_POOL_H_

#include <thread>
#include <condition_variable>

#include "magio-v3/core/coro_context.h"
#include "magio-v3/core/execution.h"
#include "magio-v3/core/noncopyable.h"

namespace magio {

template<typename>
class Coro;

class CoroContext;

class ThreadPool: Noncopyable, public Executor {
public:
    enum State {
        Stopping,
        Running,
        PendingDestroy
    };

    ThreadPool(size_t thread_num);

    ~ThreadPool();

    void start();

    void stop();

    void wait();

    template<typename Rep, typename Per>
    void wait_for(const std::chrono::duration<Rep, Per>& dur) {
        std::this_thread::sleep_for(dur);
    }

    template<typename Clock, typename Dur>
    void wait_until(const std::chrono::time_point<Clock, Dur>& tp) {
        std::this_thread::sleep_until(tp);
    }

    void execute(Task&& task) override;

    template<typename Func, typename...Args>
    Coro<std::invoke_result_t<Func, Args...>> spawn_blocking(Func&& func, Args&&...args) {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        CoroContext* ctx = LocalContext;
        std::optional<VoidToUnit<ReturnType>> result;
        co_await GetCoroutineHandle(
            [&](std::coroutine_handle<> h) mutable {
                execute([
                    &result, h, ctx,
                    func = std::forward<Func>(func), 
                    tuple = std::make_tuple(std::forward<Args>(args)...)
                ]() mutable {
                    if constexpr (std::is_void_v<ReturnType>) {
                        std::apply([&func](auto&&...args) {
                            return func(args...);
                        }, tuple);
                    } else {
                        result.emplace(std::apply([&func](auto&&...args) {
                            return func(args...);
                        }, tuple));
                    }
                    ctx->wake_in_context(h);
                });
            }
        );

        if constexpr (std::is_void_v<ReturnType>) {
            co_return;
        } else {
            co_return std::move(result.value());
        }
    }

private:
    void run_in_background();

    std::once_flag once_flag_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable wait_cv_;

    State state_ = Stopping;
    bool wait_flag_ = false;
    std::deque<Task> tasks_;

    std::vector<std::thread> threads_;
};

}

#endif