#include <cassert>
#include <iostream>
#include "magio/coro/CoSpawn.h"
#include "magio/coro/Coro.h"
#include "magio/EventLoop.h"

using namespace std;
using namespace magio;

Coro<void, int(int)> gen_num() {
    int sum = 0;
    for (int i = 0; i < 3; ++i) {
        sum += co_yield i;
    }
    cout << sum << '\n';
}

Coro<void> amain() {
    auto gen = gen_num();
    cout << gen(114514) << "\n";
    cout << gen(5) << "\n";
    cout << gen(10) << "\n";
    cout << gen(20) << "\n";
    co_return;
}

int main() {
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}