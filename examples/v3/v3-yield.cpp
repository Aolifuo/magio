#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> test() {
    const size_t T = 1e7;
    auto beg = TimerClock::now();
    for (size_t i = 0; i < T; ++i) {
        co_await this_coro::yield;
    }
    auto dif = TimerClock::now() - beg;
    M_INFO("{}", dif /  T);
    this_context::stop();
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(test());
    ctx.start();
}