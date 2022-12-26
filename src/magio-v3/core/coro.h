#ifndef MAGIO_CORE_CORO_H_
#define MAGIO_CORE_CORO_H_

#include <optional>
#include <exception>

#include "magio-v3/core/utils.h"
#include "magio-v3/core/this_context.h"
#include "magio-v3/core/noncopyable.h"

namespace magio {

template<typename = void>
class Coro;

template<typename T>
class Awaitable {
public:
    using CoroutineHandle = typename Coro<T>::CoroutineHandle;

    Awaitable(CoroutineHandle h)
        : handle_(h) { }

    bool await_ready() { 
        return false; 
    }

    template<typename PT>
    void await_suspend(std::coroutine_handle<PT> prev_h) {
        handle_.promise().prev_handle = prev_h;
        this_context::wake_in_context(handle_); // wake main then prev
    }

    T await_resume() {
        if (handle_.promise().eptr) {
            std::rethrow_exception(handle_.promise().eptr);
        }

        if constexpr (!std::is_void_v<T>) {
            return std::move(handle_.promise().value.value());
        }
    }

private:
    CoroutineHandle handle_;
};

template<typename Func>
class GetCoroutineHandle {
public:
    GetCoroutineHandle(Func func)
        : func_(std::move(func))
    { }

    bool await_ready() { 
        return false; 
    }

    void await_suspend(std::coroutine_handle<> prev_h) {
        func_(prev_h);
    }

    void await_resume() { }

private:
    Func func_;
};

template<typename Func>
GetCoroutineHandle(Func func) -> GetCoroutineHandle<Func>;

template<typename T = void>
class FinalSuspend {
public:
    using CoroutineHandle = typename Coro<T>::CoroutineHandle;

    bool await_ready() noexcept { 
        return false; 
    }

    void await_suspend(CoroutineHandle self_h) noexcept {
        if (self_h.promise().prev_handle) {
            self_h.promise().prev_handle.resume();
        } else if (self_h.promise().callback) {
            if constexpr (std::is_void_v<T>) {
                self_h.promise().callback(self_h.promise().eptr, Unit{});
            } else {
                if (self_h.promise().eptr) {
                    self_h.promise().callback(self_h.promise().eptr, {});
                } else {
                    self_h.promise().callback(self_h.promise().eptr, std::move(self_h.promise().value.value()));
                }
            }
        }

        self_h.destroy();
    }

    void await_resume() noexcept {

    }

private:
};

template<typename Return>
class Coro {
    friend class CoroContext;
    friend void this_context::spawn(Coro<Return> coro, CoroCompletionHandler<Return> &&handler);

public:
    struct promise_type;

    using CoroutineHandle = std::coroutine_handle<promise_type>;

    Coro(CoroutineHandle h)
        : handle_(h) { }

    ~Coro() { }

    struct promise_type {
        Coro get_return_object() {
            return {CoroutineHandle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { 
            return {};
        };

        FinalSuspend<Return> final_suspend() noexcept {
            return {};
        }

        void return_value(Return val) {
            value = std::move(val);
        }

        void unhandled_exception() {
            eptr = std::current_exception();
        }

        std::coroutine_handle<> prev_handle;
        std::exception_ptr eptr;
        std::optional<Return> value;
        CoroCompletionHandler<Return> callback;
    };

    CoroutineHandle handle() const {
        return handle_;
    }

    auto operator co_await() const {
        return Awaitable<Return>{handle_};
    }

private:
    void set_callback(CoroCompletionHandler<Return>&& handler) const {
        handle_.promise().callback = std::move(handler);
    }

    CoroutineHandle handle_;
};

template<>
class Coro<void> {
    friend class CoroContext;
    friend void this_context::spawn(Coro<> coro, CoroCompletionHandler<void> &&handler);

public:
    struct promise_type;

    using CoroutineHandle = std::coroutine_handle<promise_type>;

    Coro(CoroutineHandle h)
        : handle_(h) { }

    ~Coro() { }

    struct promise_type {
        Coro get_return_object() {
            return {CoroutineHandle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { 
            return {};
        };

        FinalSuspend<> final_suspend() noexcept {
            return {};
        }

        void return_void() { }

        void unhandled_exception() {
            eptr = std::current_exception();
        }
        
        std::coroutine_handle<> prev_handle;
        std::exception_ptr eptr;
        CoroCompletionHandler<void> callback;
    };

    CoroutineHandle handle() const {
        return handle_;
    }

    auto operator co_await() const {
        return Awaitable<void>{handle_};
    }

private:
    void set_callback(CoroCompletionHandler<void>&& handler) const {
        handle_.promise().callback = std::move(handler);
    }

    CoroutineHandle handle_;
};

template<typename...Ts>
inline Coro<RemoveVoidTuple<Ts...>> join(Coro<Ts>...coros);

template<typename...Ts>
inline Coro<> select(Coro<Ts>...coros);

template<typename...Ts>
inline Coro<RemoveVoidTuple<Ts...>> series(Coro<Ts>...coros);

namespace this_coro {

class Yield {
public:
    bool await_ready() { 
        return false; 
    }

    void await_suspend(std::coroutine_handle<> prev_h) {
        this_context::wake_in_context(prev_h);
    }

    void await_resume() { }
};

inline Yield yield;

template<typename Rep, typename Per>
inline Coro<> sleep_for(const std::chrono::duration<Rep, Per>& dur);

inline Coro<> sleep_until(const TimerClock::time_point& tp);

}

}

#endif