#pragma once

#include <concepts>
#include <coroutine>
#include <exception>
#include "magio/core/Execution.h"
#include "magio/core/Error.h"
#include "magio/core/MaybeUninit.h"
#include "magio/utils/Function.h"

namespace magio {

struct None {};

namespace detail {

template<typename Yield>
concept YieldType =
    std::is_function_v<Yield> &&  
    (
        std::is_object_v<typename FunctorTraits<Yield>::ReturnType> ||
        std::is_void_v<typename FunctorTraits<Yield>::ReturnType>
    ) &&  
    (
        FunctorTraits<Yield>::Arguments::Length == 1 ||
        FunctorTraits<Yield>::Arguments::Length == 0
    );

template<typename Yield>
using YieldReturnType = std::conditional_t<
    std::is_void_v<typename FunctorTraits<Yield>::ReturnType>,
    None,
    typename FunctorTraits<Yield>::ReturnType
>;

template<typename Yield>
using YieldReceiveType = std::conditional_t<
    FunctorTraits<Yield>::Arguments::Length == 0,
    void,
    typename FunctorTraits<Yield>::Arguments::template At<0>
>;

template<typename Ret>
struct CoroCompletionHandler {
    using type = std::function<void(Expected<Ret, std::exception_ptr>)>;
};

template<>
struct CoroCompletionHandler<void> {
    using type = std::function<void(Expected<Unit, std::exception_ptr>)>;
};

}

template<typename Ret>
using CoroCompletionHandler = typename detail::CoroCompletionHandler<Ret>::type;

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

template<typename Ret>
struct YieldSuspend {
    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> prev_h) noexcept {
        
    }

    Ret await_resume() noexcept {
        
    }
};

template<typename Ret, typename Yield, typename Awaitable>
struct PromiseTypeBase;

template<typename Ret, typename Yield, typename Awaitable>
struct PromiseTypeBase {
    Awaitable get_return_object() {
            return {
                std::coroutine_handle<PromiseTypeBase>::from_promise(*this),
                { }
            };
        }

    auto initial_suspend() {
        return std::suspend_always();
    }

    auto final_suspend() noexcept {
        if (completion_handler) {
            if (eptr) {
                completion_handler(eptr);
            } else {
                completion_handler(storage.unwrap());
            }
        }

        if (previous) {
            previous.resume();
        }

        return FinalSuspend(auto_destroy);
    }

    void return_value(Ret value) {
        storage = std::move(value);
    }

    YieldSuspend<detail::YieldReceiveType<Yield>>
    yield_value(detail::YieldReturnType<Yield> value) {
        return {};
    }

    void unhandled_exception() {
        eptr = std::current_exception(); 
    }

    Ret get_value() {
        return storage.unwrap();
    }

    bool auto_destroy = true;
    std::exception_ptr eptr;
    magio::AnyExecutor executor;
    std::coroutine_handle<> previous;
    CoroCompletionHandler<Ret> completion_handler;
    MaybeUninit<Ret> storage;
};

template<typename Yield, typename Awaitable>
struct PromiseTypeBase<void, Yield, Awaitable> {
    Awaitable get_return_object() {
        return {
            std::coroutine_handle<PromiseTypeBase>::from_promise(*this),
            { }
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
            previous.resume();
        } 
        
        return FinalSuspend(auto_destroy);
    }

    void return_void() { }

    YieldSuspend<detail::YieldReceiveType<Yield>>
    yield_value(detail::YieldReturnType<Yield> value) {
        return {};
    }

    void unhandled_exception() {
        eptr = std::current_exception(); 
    }

    bool auto_destroy = true;
    std::exception_ptr eptr;
    magio::AnyExecutor executor;
    std::coroutine_handle<> previous;
    CoroCompletionHandler<void> completion_handler;
};

}