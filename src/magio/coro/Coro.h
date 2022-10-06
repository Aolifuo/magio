#pragma once

#include "magio/coro/UseCoro.h"
#include "magio/utils/ScopeGuard.h"

template<typename Ret, typename...Ts>
struct std::experimental::coroutine_traits<magio::Coro<Ret>, Ts...> {
    using promise_type = magio::PromiseTypeBase<Ret, magio::Coro<Ret>>;
    using handle_type = std::experimental::coroutine_handle<promise_type>;
};

namespace magio {

namespace this_coro {

struct GetExecutor {
    bool await_ready() { return false; }

    template<typename PT>
    auto await_suspend(coroutine_handle<PT> prev_h) {
        executor = prev_h.promise().executor;
        prev_h.resume();
    }

    auto await_resume() {
        return executor;
    }

    AnyExecutor executor;
};

inline GetExecutor executor;

}

inline Coro<> timeout(size_t ms) {
    auto exe = co_await this_coro::executor;
    co_await Coro<>{
        [exe, ms](coroutine_handle<> h) mutable {
            exe.set_timeout(ms, [h]() mutable {
                h.resume();
            });
        }
    };
}

}



