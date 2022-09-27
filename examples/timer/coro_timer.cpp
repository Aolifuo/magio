#include "magio/EventLoop.h"
#include "magio/timer/Timer.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Coro.h"
#include "magio/coro/ThisCoro.h"

using namespace magio;
using namespace std;
 
Coro<void> amain() {
    Timer timer(co_await this_coro::executor, 1000);

    printf("Hello ");
    co_await timer.async_wait(use_coro);
    printf("world\n");
}

int main() {
    EventLoop loop;

    co_spawn(loop.get_executor(), amain(), detached);

    loop.run();
}