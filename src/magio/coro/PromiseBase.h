#pragma once

#include <coroutine>
#include <cstdio>
#include <exception>
#include "magio/core/Execution.h"
#include "magio/core/MaybeUninit.h"

namespace magio {

using CoroCompletionHandler = std::function<void(std::exception_ptr)>;

struct FinalSuspend {
    FinalSuspend(bool flag) {
        auto_destroy = flag;
    }

    bool await_ready() noexcept {
        return auto_destroy;
    }

    void await_suspend(std::coroutine_handle<> prev_h) noexcept {
        
    }

    void await_resume() noexcept {
        
    }

    bool auto_destroy;
};

template<typename Ret, typename Awaitable>
struct PromiseTypeBase;

template<typename Ret, typename Awaitable>
struct PromiseTypeBase {
    Awaitable get_return_object() {
            return {
                std::coroutine_handle<PromiseTypeBase>::from_promise(*this),
                [=](std::coroutine_handle<> h) {
                    executor.post([=] { h.resume(); });
                }
            };
        }

    auto initial_suspend() {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        if (completion_handler) {
            completion_handler(eptr);
        }

        if (previous) {
            executor.post([=] { previous.resume(); });
        }

        return FinalSuspend(auto_destroy);
    }

    void return_value(Ret value) {
        storage = std::move(value);
    }

    void unhandled_exception() {
        auto_destroy = true;
        eptr = std::current_exception(); 
    }

    Ret get_value() {
        return std::move(storage.unwrap());
    }

    bool auto_destroy = false;
    std::exception_ptr eptr;
    magio::AnyExecutor executor;
    std::coroutine_handle<> previous;
    CoroCompletionHandler completion_handler;
    MaybeUninit<Ret> storage;
};

template<typename Awaitable>
struct PromiseTypeBase<void, Awaitable> {
    Awaitable get_return_object() {
        return {
            std::coroutine_handle<PromiseTypeBase>::from_promise(*this),
            [=](std::coroutine_handle<> h) {
                executor.post([=] { h.resume(); });
            }
        };
    }

    auto initial_suspend() {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        if (completion_handler) {
            completion_handler(eptr);
        }

        if (previous) {
            executor.post([=] { previous.resume(); });
        } 
        
        return FinalSuspend(auto_destroy);
    }

    void return_void() { }

    void unhandled_exception() {
        auto_destroy = true;
        eptr = std::current_exception(); 
    }

    bool auto_destroy = false;
    std::exception_ptr eptr;
    magio::AnyExecutor executor;
    std::coroutine_handle<> previous;
    CoroCompletionHandler completion_handler;
};

}