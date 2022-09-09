#pragma once

#include "magio/coro/PromiseBase.h"

namespace magio {

using ResumeHandler = std::function<void(std::coroutine_handle<>)>;

struct NoReturn {};

template<typename Ret>
struct Coro {
    using value_type = Ret;

    Coro(std::coroutine_handle<PromiseTypeBase<Ret, Coro>> h, ResumeHandler resume_h)
        : main_h_(h)
        , resume_handler_(std::move(resume_h)) 
    {}

    Coro(Coro &&) = default;
    Coro& operator=(Coro &&) = default;

    bool await_ready() { return false; }

    template<typename PT>
    void await_suspend(std::coroutine_handle<PT> prev_h) {
        main_h_.promise().executor = prev_h.promise().executor;
        main_h_.promise().previous = prev_h;
        main_h_.resume();
    }

    Ret await_resume() {
        if(main_h_.promise().eptr) {
            std::rethrow_exception(main_h_.promise().eptr);
        }
        return get_value();
    }

    void set_completion_handler(CoroCompletionHandler handler) {
        main_h_.promise().completion_handler = std::move(handler);
    }

    void wake(AnyExecutor executor, bool auto_destroy) {
        main_h_.promise().executor = executor;
        main_h_.promise().auto_destroy = auto_destroy;
        executor.post([main_h_ = main_h_] { main_h_.resume(); });
    }

    auto get_value() {
        return main_h_.promise().get_value();
    }

    void destroy() {
        main_h_.destroy();
    }

    bool has_coro_handle() {
        return main_h_ != nullptr;
    }
private:
    std::coroutine_handle<PromiseTypeBase<Ret, Coro>> main_h_;
    ResumeHandler resume_handler_;
};

template<>
struct Coro<void> {
    using value_type = void;

    Coro(std::coroutine_handle<PromiseTypeBase<void, Coro>> h, ResumeHandler resume_h)
        : main_h_(h)
        , resume_handler_(std::move(resume_h)) 
    {}

    Coro(ResumeHandler resume_h)
        : resume_handler_(std::move(resume_h)) 
    {}

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

    void await_resume() {
        if (main_h_) {
            if (main_h_.promise().eptr) {
                std::rethrow_exception(main_h_.promise().eptr);
            }
        }
    }

    void set_completion_handler(CoroCompletionHandler handler) {
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
            executor.post(
                [fn = std::move(resume_handler_)] { 
                    fn(nullptr); 
                });
        }
    }

    NoReturn get_value() {
        return {};
    }

    void destroy() {
        if (main_h_ ) {
            main_h_.destroy();
        }
    }

    bool has_coro_handle() {
        return main_h_ != nullptr;
    }
private:
    std::coroutine_handle<PromiseTypeBase<void, Coro>> main_h_ = nullptr;
    ResumeHandler resume_handler_;
};

struct UseCoro { };

struct UseFuture { };

inline UseCoro use_coro;

inline UseFuture use_future;

}