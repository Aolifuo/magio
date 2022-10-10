#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;

Coro<> amain() {
    auto exe = co_await this_coro::executor;
    auto socket = co_await UdpSocket::bind("127.0.0.1", 8001);
    auto channel = make_shared<Channel<string_view, SocketAddress>>(exe);

    // send
    co_spawn(exe, [&socket, channel]() -> Coro<> {
        for (; ;) {
            auto [str, addr] = co_await channel->async_receive(use_coro);
            co_await socket.write_to(str.data(), str.length(), addr);
        }
    }());

    // receive
    array<char, 1024> buf;
    for (; ;) {
        auto [rdlen, addr] = co_await socket.read_from(buf.data(), buf.size());
        channel->async_send({buf.data(), rdlen}, addr);
    }
}

int main() {
    EventLoop loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}