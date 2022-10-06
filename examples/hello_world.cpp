#include <iostream>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace operation;

Coro<string> task1() {
    co_return "hello ";
}

Coro<string> task2() {
    co_return "world";
}

Coro<> do_tasks() {
    co_await timeout(3000);
    auto [s1, s2] = co_await (task1() && task2());
    cout << s1 << s2 << endl;
}

int main() {
    EventLoop loop;
    co_spawn(loop.get_executor(), do_tasks(), detached);
    loop.run();
}