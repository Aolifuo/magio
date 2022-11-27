#pragma once

#include "magio/coro/PromiseBase.h"
#include "magio/coro/Awaitbale.h"
#include "magio/utils/utils.h"

#ifdef __cpp_impl_coroutine
template<typename Ret, typename...Ts>
struct std::coroutine_traits<magio::Coro<Ret>, Ts...> {
    using promise_type = magio::PromiseTypeBase<Ret, magio::Coro<Ret>>;
    using handle_type = std::coroutine_handle<promise_type>;
};
#else
template<typename Ret, typename...Ts>
struct std::experimental::coroutine_traits<magio::Coro<Ret>, Ts...> {
    using promise_type = magio::PromiseTypeBase<Ret, magio::Coro<Ret>>;
    using handle_type = std::experimental::coroutine_handle<promise_type>;
};
#endif

namespace magio {

namespace detail {

struct SetTimeout {
    SetTimeout(size_t ms): ms(ms) {}

    bool await_ready() { return false; }

    template<typename PT>
    auto await_suspend(coroutine_handle<PT> prev_h) {
        prev_h.promise().timeout_ = ms;
        prev_h.promise().timeout_node_ = &prev_h.promise();
        prev_h.resume();
    }

    void await_resume() {
    }

    size_t ms;
};

struct GetExecutor {
    bool await_ready() { return false; }

    template<typename PT>
    auto await_suspend(coroutine_handle<PT> prev_h) {
        executor = prev_h.promise().executor_;
        prev_h.resume();
    }

    AnyExecutor await_resume() {
        return executor;
    }

    AnyExecutor executor;
};

struct CoroYield {
    bool await_ready() { return false; }

    template<typename PT>
    auto await_suspend(coroutine_handle<PT> prev_h) {
        prev_h.promise().executor_.async_wake(&prev_h.promise());
    }

    void await_resume() {
    }
};

struct WakeAfterTimeout {
    bool await_ready() { return false; }

    template<typename PT>
    auto await_suspend(coroutine_handle<PT> prev_h) {
        prev_h.promise().timeout_node_->wake();
    }

    void await_resume() {
    }
};

}

namespace this_coro {

inline detail::GetExecutor executor;

inline detail::CoroYield yield;

}

template<typename Ret = void>
class Coro {
public:
    Coro(coroutine_handle<PromiseTypeBase<Ret, Coro>> h)
        : main_h_(h)
    { }

    ~Coro() {
        // may leak
    }

    Coro(const Coro&) = delete;

    Coro(Coro&& other) noexcept {
        main_h_ = other.main_h_;
        other.main_h_ = {};
    }
    Coro& operator=(Coro&& other) noexcept {
        main_h_ = other.main_h_;
        other.main_h_ = {};
        return *this;
    }

    bool await_ready() { return false; }

    template<typename PT>
    void await_suspend(coroutine_handle<PT> prev_h) {
        main_h_.promise().executor_ = prev_h.promise().executor_;
        main_h_.promise().prev_ = &prev_h.promise();
        prev_h.promise().next_ = &main_h_.promise();
        main_h_.promise().timeout_ = prev_h.promise().timeout_; 
        main_h_.promise().timeout_node_ = prev_h.promise().timeout_node_;
        main_h_.promise().wake();
    }

    Ret await_resume() {
        if constexpr(!std::is_void_v<Ret>) {
            return main_h_.promise().get_value();
        } else {
            main_h_.promise().get_value();
        }
    }

    void set_completion_handler(CoroHandler<Ret>&& handler) {
        main_h_.promise().completion_handler_ = std::move(handler);
    }

    void wake(AnyExecutor executor) {
        main_h_.promise().executor_ = executor;
        executor.async_wake({&main_h_.promise()});
    }

    auto& promise() const {
        return main_h_.promise();
    }

private:
    coroutine_handle<PromiseTypeBase<Ret, Coro>> main_h_;
};

inline Coro<> sleep(size_t ms) {
    co_await Awaitable{
        [ms](AnyExecutor exe, Waker waker, size_t timeout) {
            auto id = exe.invoke_after([waker](bool exit) {
                if (exit) {
                    return;
                }

                waker.wake();
            }, ms);

            // cancel_after
            if (timeout != MAGIO_MAX_TIME) {
                exe.invoke_after([exe, id, waker](bool exit) {
                    if (exit) {
                        return;
                    }

                    if(exe.cancel(id)) {
                        waker.node_->timeout_node_->wake();
                    }
                }, timeout);
            }
        }
    };
}


template<typename Ret>
inline Coro<MaybeUninit<util::VoidToUnit<Ret>>> timeout(size_t ms, Coro<Ret> coro) {
    // completion flag
    bool flag = false;
    std::exception_ptr eptr;

    MaybeUninit<util::VoidToUnit<Ret>> ret;

    co_await Awaitable{
        [&](AnyExecutor exe, Waker waker, size_t tm) {
            tm = ms > tm ? tm : ms;
            coro.promise().timeout_ = tm;
            coro.promise().timeout_node_ = waker.node_;
            coro.set_completion_handler(
                [&flag, &eptr, &ret, waker](Expected<util::VoidToUnit<Ret>, std::exception_ptr> exp) mutable {
                    flag = true;
                    if (!exp) {
                        eptr = exp.unwrap_err();
                    } else {
                        ret = exp.unwrap();
                    }
                    waker.wake();
                });
            coro.wake(exe);
        }
    };

    if (eptr) {
        std::rethrow_exception(eptr);
    }

    if (!flag) {
        coro.promise().destroy_all();
        co_return {};
    }

    co_return ret;
}


}




