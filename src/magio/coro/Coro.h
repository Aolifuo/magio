#pragma once

#include "magio/coro/ThisCoro.h"
#include "magio/coro/UseCoro.h"
#include "magio/utils/ScopeGuard.h"

template<typename Ret, typename...Ts>
struct std::experimental::coroutine_traits<magio::Coro<Ret>, Ts...> {
    using promise_type = magio::PromiseTypeBase<Ret, magio::Coro<Ret>>;
    using handle_type = std::experimental::coroutine_handle<promise_type>;
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
            None,
            Rets
        >...
    >
> coro_join(Coro<Rets>&&...coros) {
    AnyExecutor executor = co_await this_coro::executor;
    
    auto coro_tup =
        std::make_tuple((coros.has_coro_handle() ? std::move(coros) : detail::make_coro(std::move(coros)))...);
    
    auto guard = ScopeGuard(&coro_tup, [](auto* p){
        std::apply([](auto&&...coros) {
            (coros.destroy(), ...);
        }, *p);
    });
    
    co_await Coro<void>(
        [&coro_tup, executor](coroutine_handle<> h) mutable {
            std::apply([executor](auto&&...coros) {
                (coros.wake(executor, false), ...);
            }, coro_tup);

            executor.waiting([&coro_tup, h, executor]() mutable {
                bool flag = std::apply([](auto&&...coros) {
                    return (coros.done() && ...);
                }, coro_tup);

                if (flag) {
                    executor.post([h]() mutable { h.resume(); });
                }

                return flag;
            });
        });

    auto ret_tup = std::apply(
        [](auto&&...coros) {
            return std::make_tuple((coros.get_value())...);
        }, coro_tup);

    co_return ret_tup;
}

}



