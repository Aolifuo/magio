#pragma once

#include "magio/coro/PromiseBase.h"

namespace magio {

using ResumeHandler = std::function<void(std::coroutine_handle<>)>;

template<typename Ret, typename Yield = void()>
requires detail::YieldType<Yield>
struct Coro {
    Coro(ResumeHandler&& resume_h)
        : main_h_(nullptr)
        , resume_handler_(std::move(resume_h))
    { }

    Coro(std::coroutine_handle<PromiseTypeBase<Ret, Yield, Coro>> h, ResumeHandler&& resume_h)
        : main_h_(h)
        , resume_handler_(std::move(resume_h)) 
    { }

    Coro(Coro &&) = default;
    Coro& operator=(Coro &&) = default;

    bool await_ready() { return false; }

    template<typename PT>
    void await_suspend(std::coroutine_handle<PT> prev_h) {
        if (main_h_) {
            main_h_.promise().executor = prev_h.promise().executor;
            main_h_.promise().previous = prev_h;
            main_h_.resume();
        } else {
            resume_handler_(prev_h);
        }
    }

    Ret await_resume() {
        if constexpr(!std::is_void_v<Ret>) {
            return get_value();
        } else {
            get_value();
        }
    }

    void set_completion_handler(CoroCompletionHandler<Ret>&& handler) {
        if (main_h_) {
            main_h_.promise().completion_handler = std::move(handler);
        }
    }

    void wake(AnyExecutor executor, bool auto_destroy) {
        if (main_h_) {
            main_h_.promise().executor = executor;
            main_h_.promise().auto_destroy = auto_destroy;
            executor.post([main_h_ = main_h_] { main_h_.resume(); });
        } else {
            executor.post([r = std::move(resume_handler_)] { r(nullptr); });
        }
    }

    // has promise
    auto get_value() {
        if (main_h_) {
            if (main_h_.promise().eptr) {
                std::rethrow_exception(main_h_.promise().eptr);
            }
        }

        if constexpr (!std::is_void_v<Ret>) {
            return main_h_.promise().get_value();
        } else {
            return None{};
        }
    }

    bool done() {
        return main_h_.done();
    }

    void destroy() {
        main_h_.destroy();
    }

    bool has_coro_handle() {
        return main_h_ != nullptr;
    }
private:
    std::coroutine_handle<PromiseTypeBase<Ret, Yield, Coro>> main_h_;
    ResumeHandler resume_handler_;
};

struct UseCoro { };

struct UseFuture { };

inline UseCoro use_coro;

inline UseFuture use_future;

}