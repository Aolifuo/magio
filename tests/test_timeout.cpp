#include "magio/magio.h"
#include "preload.h"

Coro<> delay(size_t ms, Handler h) {
    co_await sleep(ms);
    h();
}

Coro<> amain() {
    auto m = co_await timeout(1000, delay(0, []{ cout << "Hi\n"; }));
    if (!m) {
        cout << "timeout!\n";
    } else {
        cout << "over\n";
    }
}

int main() {
    Magico loop;
    co_spawn(loop.get_executor(), amain());
    loop.run();
}