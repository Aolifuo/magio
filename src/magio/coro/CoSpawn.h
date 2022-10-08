#pragma once

#include <future>
#include "magio/coro/Coro.h"

namespace magio {

template<typename Ret>
inline void co_spawn(AnyExecutor executor, Coro<Ret>&& coro, detail::Detached) {
    coro.wake(executor, true);
}

template<typename Ret>
inline void co_spawn(AnyExecutor executor, Coro<Ret>&& coro, CoroCompletionHandler<Ret> handler) {
    coro.set_completion_handler(std::move(handler));
    coro.wake(executor, true);
}

template<typename Ret>
inline Coro<Ret> co_spawn(AnyExecutor executor, Coro<Ret>&& coro, detail::UseCoro) {
    co_return co_await coro;
}

template<typename Ret>
inline std::future<Ret> co_spawn(AnyExecutor executor, Coro<Ret>&& coro, detail::UseFuture) {
    auto pro_ptr = std::make_shared<std::promise<Ret>>();
    auto fut = pro_ptr->get_future();
    coro.set_completion_handler(
        [pro_ptr](Expected<Ret, std::exception_ptr> exp) mutable {
            if (exp) {
                pro_ptr->set_value(exp.unwrap());
            } else {
                pro_ptr->set_exception(exp.unwrap_err());
            }
        }
    );

    coro.wake(executor, true);
    return fut;
}


}