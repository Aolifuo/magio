#ifndef MAGIO_CORE_CORO_H_
#define MAGIO_CORE_CORO_H_

#include <variant>
#include <exception>

#include "magio-v3/utils/noncopyable.h"
#include "magio-v3/core/this_context.h"

namespace magio {

#ifdef MAGIO_USE_CORO
namespace detail {

inline thread_local size_t CoroNum = 0;

class Yield {
public:
    bool await_ready() { 
        return false; 
    }

    void await_suspend(std::coroutine_handle<> prev_h) {
        this_context::queue_in_context(prev_h);
    }

    void await_resume() { }
};

}

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
        handle_.promise().is_launched = true;
        handle_.promise().prev_handle = prev_h;
        this_context::queue_in_context(handle_); // wake main then prev
    }

    T await_resume() noexcept {
        if (auto peptr = std::get_if<std::exception_ptr>(&handle_.promise().storage); peptr) {
            std::rethrow_exception(*peptr);
        }

        if constexpr (!std::is_void_v<T>) {
            return std::move(std::get<T>(handle_.promise().storage));
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

    constexpr bool await_ready() const noexcept { 
        return false; 
    }

    void await_suspend(CoroutineHandle self_h) const noexcept {
        if (self_h.promise().prev_handle) {
            self_h.promise().prev_handle.resume();
        } else if (self_h.promise().callback) {
            auto peptr = std::get_if<std::exception_ptr>(&self_h.promise().storage);
            if constexpr (std::is_void_v<T>) {
                if (peptr) {
                    self_h.promise().callback(std::move(*peptr), Unit{});
                } else {
                    self_h.promise().callback(std::exception_ptr{}, Unit{});
                }
            } else {
                if (peptr) {
                    self_h.promise().callback(std::move(*peptr), {});
                } else {
                    self_h.promise().callback(
                        std::exception_ptr{}, 
                        std::move(std::get<T>(self_h.promise().storage))
                    );
                }
            }
        }

        --detail::CoroNum;
        self_h.destroy();
    }

    constexpr void await_resume() const noexcept { }
};

template<typename Return>
class Coro: Noncopyable {
public:
    struct promise_type;

    using CoroutineHandle = std::coroutine_handle<promise_type>;

    Coro(CoroutineHandle h)
        : handle_(h) { }

    ~Coro() { 
        if (handle_ && !handle_.promise().is_launched) {
            --detail::CoroNum;
            handle_.destroy();
        }
    }

    Coro(Coro&& other) noexcept
        : handle_(other.handle_) 
    {
        other.handle_ = {};
    }

    Coro& operator=(Coro&& other) noexcept {
        handle_ = other.handle_;
        other.handle_ = {};
        return *this;
    }

    struct promise_type {
        Coro get_return_object() {
            ++detail::CoroNum;
            return {CoroutineHandle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() {
            return {};
        };

        FinalSuspend<Return> final_suspend() noexcept {
            return {};
        }

        void return_value(Return& lval) {
            storage.template emplace<Return>(lval);
        }

        void return_value(Return&& rval) {
            storage.template emplace<Return>(std::move(rval));
        }

        void unhandled_exception() {
            storage.template emplace<std::exception_ptr>(std::current_exception());
        }

        bool is_launched = false;
        std::coroutine_handle<> prev_handle;
        std::variant<std::monostate, Return, std::exception_ptr> storage;
        CoroCompletionHandler<Return> callback;
    };

    CoroutineHandle handle() const {
        return handle_;
    }

    // spawn and co_await
    void launch() {
        handle_.promise().is_launched = true;
    }
    
    void set_callback(CoroCompletionHandler<Return>&& handler) const {
        handle_.promise().callback = std::move(handler);
    }

    auto operator co_await() const {
        return Awaitable<Return>{handle_};
    }
private:
    
    CoroutineHandle handle_;
};

template<>
class Coro<void>: Noncopyable {
public:
    struct promise_type;

    using CoroutineHandle = std::coroutine_handle<promise_type>;

    Coro(CoroutineHandle h)
        : handle_(h) { }

    ~Coro() { 
        if (handle_ && !handle_.promise().is_launched) {
            --detail::CoroNum;
            handle_.destroy();
        }
    }

    Coro(Coro&& other) noexcept
        : handle_(other.handle_) 
    {
        other.handle_ = {};
    }

    Coro& operator=(Coro&& other) noexcept {
        handle_ = other.handle_;
        other.handle_ = {};
        return *this;
    }

    struct promise_type {
        Coro get_return_object() {
            ++detail::CoroNum;
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
            storage.template emplace<std::exception_ptr>(std::current_exception());
        }
        
        bool is_launched = false;
        std::coroutine_handle<> prev_handle;
        std::variant<std::monostate, std::exception_ptr> storage;
        CoroCompletionHandler<void> callback;
    };

    CoroutineHandle handle() const {
        return handle_;
    }

    void launch() {
        handle_.promise().is_launched = true;
    }
    
    void set_callback(CoroCompletionHandler<void>&& handler) const {
        handle_.promise().callback = std::move(handler);
    }

    auto operator co_await() const {
        return Awaitable<void>{handle_};
    }

private:
    CoroutineHandle handle_;
};

template<typename...Ts>
Coro<RemoveVoidTuple<Ts...>> join(Coro<Ts>...coros);

template<typename...Ts>
Coro<> select(Coro<Ts>...coros);

template<typename...Ts>
Coro<RemoveVoidTuple<Ts...>> series(Coro<Ts>...coros);

namespace this_coro {

inline detail::Yield yield;

template<typename Rep, typename Per>
Coro<> sleep_for(const std::chrono::duration<Rep, Per>& dur);

Coro<> sleep_until(const TimerClock::time_point& tp);

}
#endif

}

#endif