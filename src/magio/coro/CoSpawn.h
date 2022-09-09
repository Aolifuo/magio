#pragma once

#include "magio/coro/Coro.h"

namespace magio {

struct Detached { };

inline Detached detached;

template<typename Ret>
void co_spawn(AnyExecutor executor, Coro<Ret>&& coro, Detached) {
    coro.wake(executor, true);
}

template<typename Ret>
void co_spawn(AnyExecutor executor, Coro<Ret>&& coro, CoroCompletionHandler handler) {
    if (!coro.has_coro_handle()) {
        coro = std::move(detail::make_coro(std::move(coro)));
    }
    coro.set_completion_handler(std::move(handler));
    coro.wake(executor, true);
}

template<typename Ret>
Coro<Ret> co_spawn(AnyExecutor executor, Coro<Ret>&& coro, UseCoro) {
    co_return co_await coro;
}

}