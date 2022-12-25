#include "magio/magio.h"

using namespace std;
using namespace magio;

Coro<> amain() {
    auto exe = co_await this_coro::executor;
    SingleEvent event(exe);

    co_spawn(exe, [](SingleEvent& ev) -> Coro<> {
        co_await sleep(1000);
        ev.notify();
    }(event));

    co_await co_spawn(exe, [](SingleEvent& ev) -> Coro<> {
        co_await ev.wait();
        M_INFO("after wait");
    }(event), use_coro);
}

int main() {
    Magico loop;
    co_spawn(loop.get_executor(), amain());
    loop.run();
}
