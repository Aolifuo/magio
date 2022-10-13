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
    using type = std::function<void(Expected<Unit, std::exception_ptr>)>;
};

}

template<typename Ret>
using CoroHandler = typename detail::CoroHandler<Ret>::type;

template<typename Ret, typename Awaitable>
struct PromiseTypeBase;

struct PromiseNode {
    // be stopped
    void destroy_from_tail() {
        if (prev_) {
            auto cur = prev_;
            auto prev = cur->prev_;
            while (prev) {
                cur->destroy();
                cur = prev;
                prev = cur->prev_;
            }
            cur->destroy();
        }

        destroy();
    }

    void destroy() {
        MAGIO_DESTROY_CORO;
        handle_.destroy();
    }

    void wake() {
        handle_.resume();
    }

    std::shared_ptr<std::atomic_flag> stop_flag_;

    coroutine_handle<> handle_;
    std::exception_ptr eptr_;
    AnyExecutor executor_;

    PromiseNode* prev_ = nullptr;
    PromiseNode* next_ = nullptr;
};

template<typename Ret, typename A>
struct FinalSuspend {
    FinalSuspend(PromiseTypeBase<Ret, A>* promise)
        : promise(promise)
    { }

    bool await_ready() noexcept { return false; }

    void await_suspend(coroutine_handle<> prev_h) noexcept {
        if (promise->completion_handler_) {
            if (promise->eptr_) {
                promise->completion_handler_(promise->eptr_);
            } else {
                if constexpr (std::is_void_v<Ret>) {
                    promise->completion_handler_(Unit{});
                } else {
                    promise->completion_handler_(promise->storage_.unwrap());
                }
            }
        }

        // wake previous
        if (promise->prev_) {
            promise->prev_->wake();
        }

        promise->destroy();
    }

    void await_resume() noexcept {
        
    }
    
    PromiseTypeBase<Ret, A>* promise;
};

template<typename Ret, typename Awaitable>
struct PromiseTypeBase: PromiseNode {
    Awaitable get_return_object() {
        MAGIO_CREATE_CORO;
        handle_= coroutine_handle<PromiseTypeBase>::from_promise(*this);
        return {
            coroutine_handle<PromiseTypeBase>::from_promise(*this)
        };
    }

    auto initial_suspend() {
        return suspend_always{};
    }

    auto final_suspend() noexcept {
        return FinalSuspend(this);
    }

    void return_value(Ret value) {
        storage_ = std::move(value);
    }

    void unhandled_exception() {
        eptr_ = std::current_exception(); 
    }

    Ret get_value() {
        if (eptr_) {
            std::rethrow_exception(eptr_);
        }

        return storage_.unwrap();
    }

    CoroHandler<Ret> completion_handler_;

    MaybeUninit<Ret> storage_;
};

template<typename Awaitable>
struct PromiseTypeBase<void, Awaitable>: PromiseNode {
    Awaitable get_return_object() {
        MAGIO_CREATE_CORO;
        handle_= coroutine_handle<PromiseTypeBase>::from_promise(*this);
        return {
            coroutine_handle<PromiseTypeBase>::from_promise(*this)
        };
    }

    auto initial_suspend() {
        return suspend_always();
    }

    auto final_suspend() noexcept {
        return FinalSuspend(this);
    }

    void return_void() { }

    void unhandled_exception() {
        eptr_ = std::current_exception(); 
    }

    void get_value() {
        if (eptr_) {
            std::rethrow_exception(eptr_);
        }
    }

    CoroHandler<void> completion_handler_;
};

}