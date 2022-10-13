#pragma once

#include <future>
#include "magio/coro/Coro.h"

namespace magio {

template<typename Ret>
inline void co_spawn(AnyExecutor executor, Coro<Ret> coro, detail::Detached d = {}) {
    coro.wake(executor);
}

template<typename Ret>
inline void co_spawn(AnyExecutor executor, Coro<Ret> coro, CoroHandler<Ret> handler) {
    coro.set_completion_handler(std::move(handler));
    coro.wake(executor);
}

template<typename Ret>
inline Coro<Ret> co_spawn(AnyExecutor executor, Coro<Ret> coro, detail::UseCoro) {
    co_return co_await coro;
}

template<typename Ret>
inline std::future<Ret> co_spawn(AnyExecutor executor, Coro<Ret> coro, detail::UseFuture) {
    auto pro_ptr = std::make_shared<std::promise<Ret>>();
    auto fut = pro_ptr->get_future();
    coro.set_completion_handler(
        [pro_ptr](Expected<util::VoidToUnit<Ret>, std::exception_ptr> exp) mutable {
            if (exp) {
                if constexpr (std::is_void_v<Ret>) {
                    pro_ptr->set_value();
                } else {
                    pro_ptr->set_value(exp.unwrap());
                }
            } else {
                pro_ptr->set_exception(exp.unwrap_err());
            }
        }
    );

    coro.wake(executor);
    return fut;
}

}