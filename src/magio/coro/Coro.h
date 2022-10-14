#pragma once

#include "magio/coro/PromiseBase.h"
#include "magio/coro/Awaitbale.h"
#include "magio/execution/Execution.h"

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

namespace util {

template<typename T>
using VoidToUnit = std::conditional_t<std::is_void_v<T>, Unit, T>;

}

namespace this_coro {

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

template<typename Ret>
struct Attach {
    Attach(Coro<Ret>& coro): coro(coro) {

    }

    bool await_ready() { return false; }

    template<typename PT>
    auto await_suspend(coroutine_handle<PT> prev_h) {
        if (!prev_h.promise().next_) {
            prev_h.promise().next_ = &coro.promise();
            coro.promise().prev_ = &prev_h.promise();
        }
        prev_h.promise().wake();
    }

    auto await_resume() {
        
    }

    Coro<Ret>& coro;
};

template<typename T>
Attach(Coro<T>&) -> Attach<T>;

inline GetExecutor executor;

}

template<typename Ret = void>
class Coro {
public:
    Coro(coroutine_handle<PromiseTypeBase<Ret, Coro>> h)
        : main_h_(h)
    { }

    ~Coro() {
        if (!main_h_) {
            return;
        }

        // TODO
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
        prev_h.promise().next_ = &main_h_.promise();
        main_h_.promise().prev_ = &prev_h.promise();
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
        executor.post([main_h_ = main_h_] { main_h_.promise().wake(); });
    }

    void stop() {
        main_h_.promise().stop_flag_->test_and_set();
    }

    auto& promise() const {
        return main_h_.promise();
    }

private:
    coroutine_handle<PromiseTypeBase<Ret, Coro>> main_h_;
};

inline Coro<> sleep(size_t ms) {
    co_await Awaitable{
        [ms](AnyExecutor exe, Waker waker) {
            exe.set_timeout(ms, [waker] {
                waker.try_wake();
            });
        }
    };
}

}




