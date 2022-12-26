#include <iostream>
#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> coro1(SingleEvent& ch) {
    error_code ec;
    co_await ch.send(ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }
    co_return;
}

Coro<> coro2(SingleEvent& ch) {
    error_code ec;
    co_await ch.receive(ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }
    M_INFO("{}", "recv!");
}

int main() {
    CoroContext ctx(100);
    SingleEvent ch;
    this_context::spawn(join(coro1(ch), coro2(ch)));
    ctx.start();
}