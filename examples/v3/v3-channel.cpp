#include <iostream>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

static Channel<string> ch(2);

Coro<> coro_send(string str) {
    co_await ch.send(str);
    M_INFO("send {}", str);
}

Coro<> coro_recv() {
    string msg = co_await ch.receive();
    M_INFO("recv {}", msg);
}

Coro<> amain(MultithreadedContexts& ctxs) {
    for (size_t i = 0; i < 4; ++i) {
        ctxs.next_context().spawn(coro_send(to_string(i)));
    }

    co_await this_coro::sleep_for(1s);
    for (size_t i = 0; i < 4; ++i) {
        ctxs.next_context().spawn(coro_recv());
    }

    co_await this_coro::sleep_for(2s);
    this_context::stop();
}

int main() {
    CoroContext base(128);
    MultithreadedContexts ctxs(4, 128);
    base.spawn(amain(ctxs));
    ctxs.start_all();
}