#pragma once

#include <concepts>
#include <exception>
#include "magio/dev/Resource.h"
#include "magio/coro/Fwd.h"
#include "magio/coro/Config.h"
#include "magio/core/Error.h"
#include "magio/core/MaybeUninit.h"
#include "magio/execution/Execution.h"

namespace magio {

namespace detail {

template<typename Ret>
struct CoroHandler {
    using type = std::function<void(Expected<Ret, std::exception_ptr>)>;
};

template<>
struct CoroHandler<void> {
    using type = std::function<void(std::exception_ptr)>;
};

}

template<typename Ret>
using CoroHandler = typename detail::CoroHandler<Ret>::type;

template<typename Ret, typename Awaitable>
struct PromiseTypeBase;

template<typename Ret, typename A>
struct FinalSuspend {
    FinalSuspend(bool flag, PromiseTypeBase<Ret, A>& pro)
        : auto_destroy(flag)
        , promise(pro)
    {

    }

    bool await_ready() noexcept { return false; }

    void await_suspend(coroutine_handle<> prev_h) noexcept {
        if (promise.completion_handler) {
            if (promise.eptr) {
                promise.completion_handler(promise.eptr);
            } else {
                if constexpr (std::is_void_v<Ret>) {
                    promise.completion_handler(promise.eptr);
                } else {
                    promise.completion_handler(promise.storage.unwrap());
                }
            }
        }

        if (promise.previous) {
            promise.previous.resume();
        }

        if (auto_destroy) {
            MAGIO_DESTROY_CORO;
            prev_h.destroy();
        }
    }

    void await_resume() noexcept {
        
    }

    bool auto_destroy;
    PromiseTypeBase<Ret, A>& promise;
};

template<typename Ret, typename Awaitable>
struct PromiseTypeBase {
    Awaitable get_return_object() {
        MAGIO_CREATE_CORO;
        return {
            coroutine_handle<PromiseTypeBase>::from_promise(*this),
            { }
        };
    }

    auto initial_suspend() {
        return suspend_always{};
    }

    auto final_suspend() noexcept {
        return FinalSuspend(auto_destroy, *this);
    }

    void return_value(Ret value) {
        storage = std::move(value);
    }

    void unhandled_exception() {
        eptr = std::current_exception(); 
    }

    Ret get_value() {
        return storage.unwrap();
    }

    bool auto_destroy = true;
    std::exception_ptr eptr;
    AnyExecutor executor;
    coroutine_handle<> previous;
    CoroHandler<Ret> completion_handler;

    MaybeUninit<Ret> storage;
};

template<typename Awaitable>
struct PromiseTypeBase<void, Awaitable> {
    Awaitable get_return_object() {
        MAGIO_CREATE_CORO;
        return {
            coroutine_handle<PromiseTypeBase>::from_promise(*this),
            { }
        };
    }

    auto initial_suspend() {
        return suspend_always();
    }

    auto final_suspend() noexcept {
        return FinalSuspend(auto_destroy, *this);
    }

    void return_void() { }

    void unhandled_exception() {
        eptr = std::current_exception(); 
    }

    bool auto_destroy = true;
    std::exception_ptr eptr;
    AnyExecutor executor;
    coroutine_handle<> previous;
    CoroHandler<void> completion_handler;
};

}