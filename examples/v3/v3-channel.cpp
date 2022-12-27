#include <iostream>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

static Channel<string> ch;

Coro<> coro_send(string str) {
    co_await ch.send(str);
    co_return;
}

Coro<> coro_recv() {
    string msg = co_await ch.receive();
    M_INFO("recv {}", msg);
}

Coro<> amain(MultithreadedContexts& ctxs) {
    for (size_t i = 0; i < 4; ++i) {
        ctxs.next_context().spawn(coro_recv());
    }

    for (size_t i = 0; i < 4; ++i) {
        co_await this_coro::sleep_for(1s);
        ctxs.next_context().spawn(coro_send(to_string(i)));
    }


    co_return;
}

int main() {
    CoroContext base(128);
    MultithreadedContexts ctxs(4, 128);
    base.spawn(amain(ctxs));
    ctxs.start_all();
}