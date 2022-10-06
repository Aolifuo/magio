#include "magio/timer/Timer.h"
#include "magio/coro/Coro.h"

namespace magio {

Coro<void> Timer::async_wait(UseCoro) {
    co_await Coro<void> {
        [=](coroutine_handle<> h) {
            executor_.set_timeout(timeout_, [=]() mutable {
                if (h) {
                    h.resume();
                }
            });
        }
    };
}

}


