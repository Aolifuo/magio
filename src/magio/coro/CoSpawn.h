#pragma once

#include <future>
#include "magio/coro/Coro.h"

namespace magio {

struct Detached { };

inline Detached detached;

template<typename Ret, typename Yield>
void co_spawn(AnyExecutor executor, Coro<Ret, Yield>&& coro, Detached) {
    coro.wake(executor, true);
}

template<typename Ret, typename Yield>
void co_spawn(AnyExecutor executor, Coro<Ret, Yield>&& coro, CoroCompletionHandler<Ret>&& handler) {
    if (!coro.has_coro_handle()) {
        coro = detail::make_coro(std::move(coro));
    }
    coro.set_completion_handler(std::move(handler));
    coro.wake(executor, true);
}

template<typename Ret, typename Yield>
Coro<Ret> co_spawn(AnyExecutor executor, Coro<Ret, Yield>&& coro, UseCoro) {
    co_return co_await coro;
}

}