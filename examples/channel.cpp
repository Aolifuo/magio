#include <iostream>
#include "magio/magio.h"

using namespace std;
using namespace magio;

void func1(shared_ptr<Channel<int, string>> chan) {
    for (size_t i = 0; i < 5; ++i) {
        chan->async_receive([](int n, string str) {
            printf("%s receive %d %s\n", "func1", n, str.c_str());
        });
    }
}

Coro<void> func2(shared_ptr<Channel<int, string>> chan) {
    for (size_t i = 0; i < 5; ++i) {    
        auto [n, str] = co_await chan->async_receive(use_coro);
        printf("%s receive %d %s\n", "func2", n, str.c_str());
    }
}

int main() {
    ThreadPool pool(8);

    auto chan = make_shared<Channel<int, string>>(pool.get_executor());
    for (size_t i = 0; i < 10; ++i) {
        chan->async_send(i, "from main");
    }
    co_spawn(pool.get_executor(), [chan]{ func1(chan); });
    co_spawn(pool.get_executor(), func2(chan), detached);
    
    pool.join();
}