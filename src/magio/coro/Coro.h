#pragma once

#include "magio/coro/ThisCoro.h"
#include "magio/coro/UseCoro.h"
#include <mutex>

template<typename Ret, typename...Ts>
struct std::coroutine_traits<magio::Coro<Ret>, Ts...> {
    using promise_type = magio::PromiseTypeBase<Ret, magio::Coro<Ret>>;
    using handle_type = std::coroutine_handle<promise_type>;
};

namespace magio {

namespace detail {

template<typename Ret>
Coro<Ret> make_coro(Coro<Ret>&& coro) {
    co_return co_await coro;
}

}

template<typename...Rets>
Coro<
    std::tuple<
        std::conditional_t<
            std::is_void_v<Rets>,
            NoReturn,
            Rets
        >...
    >
> coro_join(Coro<Rets>&&...coros) {
    std::mutex m;
    int num = sizeof...(Rets);
    std::exception_ptr eptr;
    auto executor = co_await this_coro::executor;
    
    auto coro_tup = std::make_tuple((coros.has_coro_handle() ? std::move(coros) : detail::make_coro(std::move(coros)))...);

    co_await Coro<void>(
        [&](std::coroutine_handle<> h) mutable {
            std::apply([&](auto&&...coros) mutable {
                (coros.set_executor(executor), ...);
                (coros.set_completion_handler([&m, &num, &eptr, h](std::exception_ptr e) {
                    std::lock_guard lk(m);
                    if (num == -1) {
                        return;
                    }

                    if (e) {
                        num = -1;
                        eptr = e;
                        h.resume();
                        return;
                    }

                    if (--num == 0) {
                        h.resume();
                    }
                }), ...);
                (coros.wake(false), ...);
            }, coro_tup);
        }
    );

    if (eptr) {
        // destroy ?
        std::rethrow_exception(eptr);
    }

    auto ret_tup = std::apply(
        [](auto&&...coros) {
            return std::make_tuple((coros.get_value())...);
        }, coro_tup);
    
    std::apply([](auto&&...coros) {
        (coros.destroy(), ...);
    }, coro_tup);

    co_return ret_tup;
}

}



