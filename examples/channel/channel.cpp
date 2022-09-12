#include "magio/channel/Channel.h"
#include "magio/ThreadPoolv2.h"
#include <iostream>

using namespace std;
using namespace magio;

void func(shared_ptr<Channel<int, string>> ch) {
    ch->async_send(10, "hello");
    ch->async_send(20, "world");
}

int main() {
    ThreadPoolv2 pool(10);
    auto channel = make_shared<Channel<int, string>>(pool.get_executor());

    pool.post([=] { func(channel); });
    pool.post([=]{
        channel->async_receive([](int n, string str) {
            cout << this_thread::get_id() << " receive " << n << " and " << str << '\n';
        });
    });

    channel->async_receive([](int n, string str) {
        cout << this_thread::get_id() << " receive " << n << " and " << str << '\n';
    });

    pool.join();
}