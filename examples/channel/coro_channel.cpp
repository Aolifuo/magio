#include "magio/channel/Channel.h"
#include "magio/ThreadPool.h"
#include "magio/coro/CoSpawn.h"
#include <iostream>

using namespace std;
using namespace magio;

struct NoCopy {
    NoCopy() = default;
    NoCopy(NoCopy&&) = default;
    NoCopy& operator=(NoCopy&&) = default;
};

Coro<void> func1(std::shared_ptr<Channel<NoCopy, string>> ch) {
    ch->async_send(NoCopy{}, "hey");

    auto [_, str] = co_await ch->async_receive(use_coro);
    cout << __func__ << " " << str << '\n';
}

Coro<void> func2(std::shared_ptr<Channel<NoCopy, string>> ch) {

    auto [_, str] = co_await ch->async_receive(use_coro);
    cout << __func__ << " " << str << '\n';
}

int main() {
    ThreadPool pool(10);
    
    auto channel = std::make_shared<Channel<NoCopy, string>>();
    channel->async_send(NoCopy{}, "hi");
    channel->async_send(NoCopy{}, "hello");
    co_spawn(
        pool.get_executor(), 
        coro_join(func1(channel), func2(channel)),
        detached
    );

    channel->async_receive([](NoCopy _, string str) {
        cout << "main " << str << '\n';
    });

    pool.join();
}