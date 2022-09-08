#include "magio/EventLoop.h"
#include "magio/Timer.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/ThisCoro.h"

using namespace std;
using namespace magio;

Coro<void> amain() {
    auto exe = co_await this_coro::executor;
    Timer timer(exe, 3000);

    co_await co_spawn(exe, timer.async_wait(use_coro), use_coro);
    std::printf("ok\n");
}

int main() {
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}