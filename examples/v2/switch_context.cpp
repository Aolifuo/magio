#include "magio/magio.h"

using namespace magio;
using namespace std;

constexpr size_t SwitchTimes = 1e7;

Coro<> task() {
    auto start = chrono::steady_clock::now();

    for (size_t i = 0; i < SwitchTimes; ++i) {
        co_await this_coro::yield;
    }

    auto end = chrono::steady_clock::now();
    M_INFO("{}", (end - start) / SwitchTimes);
}

int main() {
    Magico loop;
    co_spawn(loop.get_executor(), task());
    loop.run();
}