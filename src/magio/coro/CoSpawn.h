#pragma once

#include <future>
#include "magio/coro/Coro.h"

namespace magio {

template<typename Ret>
void co_spawn(AnyExecutor executor, Coro<Ret>&& coro, Detached) {
    coro.wake(executor, true);
}

template<typename Ret>
void co_spawn(AnyExecutor executor, Coro<Ret>&& coro, CoroCompletionHandler<Ret> handler) {
    coro.set_completion_handler(std::move(handler));
    coro.wake(executor, true);
}

template<typename Ret>
Coro<Ret> co_spawn(AnyExecutor executor, Coro<Ret>&& coro, UseCoro) {
    co_return co_await coro;
}

}