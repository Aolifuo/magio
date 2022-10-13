#include "magio/timer/Timer.h"
#include "magio/coro/Coro.h"

namespace magio {

Coro<void> Timer::async_wait(detail::UseCoro) {
    co_await Awaitable {
        [=](AnyExecutor exe, Waker waker) {
            executor_.set_timeout(timeout_, [=] {
                waker.try_wake();
            });
        }
    };
}

}


