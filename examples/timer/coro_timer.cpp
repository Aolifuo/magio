#include "magio/EventLoop.h"
#include "magio/timer/Timer.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Coro.h"
#include "magio/coro/ThisCoro.h"

magio::Coro<void> amain() {
    auto executor = co_await magio::this_coro::executor;
    magio::Timer timer(executor, 2000);

    std::printf("Hello ");
    co_await timer.async_wait(magio::use_coro);
    std::printf("world\n");
}

int main() {
    magio::EventLoop loop;

    magio::co_spawn(loop.get_executor(), amain(), magio::detached);

    loop.run();
}