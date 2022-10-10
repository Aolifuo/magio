#pragma once

#include "magio/coro/PromiseBase.h"

namespace magio {

using ResumeHandler = std::function<void(coroutine_handle<>)>;

template<typename Ret = void>
struct Coro {
    Coro(ResumeHandler&& resume_h)
        : main_h_(nullptr)
        , resume_handler_(std::move(resume_h))
    { }

    Coro(coroutine_handle<PromiseTypeBase<Ret, Coro>> h, ResumeHandler&& resume_h)
        : main_h_(h)
        , resume_handler_(std::move(resume_h)) 
    { }

    Coro(const Coro&) = delete;
    Coro(Coro&&) noexcept = default;
    Coro& operator=(Coro&&) noexcept = default;

    bool await_ready() { return false; }

    template<typename PT>
    void await_suspend(coroutine_handle<PT> prev_h) {
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

    void set_completion_handler(CoroHandler<Ret>&& handler) {
        if (main_h_) {
            main_h_.promise().completion_handler = std::move(handler);
        }
    }

    void wake(AnyExecutor executor, bool auto_destroy) {
        if (main_h_) {
            main_h_.promise().executor = executor;
            main_h_.promise().auto_destroy = auto_destroy;
            executor.post([main_h_ = main_h_]() mutable { main_h_.resume(); });
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
            return Unit{};
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
    coroutine_handle<PromiseTypeBase<Ret, Coro>> main_h_;
    ResumeHandler resume_handler_;
};


}