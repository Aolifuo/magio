#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Coro.h"
#include "magio/coro/ThisCoro.h"
#include <cstdio>

using namespace magio;

Coro<void> foo() {
    std::printf("hi\n");
    co_return;
}

Coro<void> amain() {
    std::printf("begin amain\n");
    //auto exe = co_await this_coro::executor;
    co_await foo();
    // co_spawn(exe, foo(), detached);
    co_return;
}

int main() {
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}