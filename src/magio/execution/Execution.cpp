#include "magio/coro/Awaitbale.h"
#include "magio/execution/Execution.h"

namespace magio {

void AnyExecutor::async_wake(Waker waker) const {
    context_->async_wake(waker);
}

TimerID AnyExecutor::invoke_after(TimerHandler&& handler, size_t ms) const {
    return context_->invoke_after(std::move(handler), ms);
}

}