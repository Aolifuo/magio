#include <array>
#include <iostream>
#include "magio/Runtime.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/tcp/TcpClient.h"

using namespace std;
using namespace magio;

Coro<void> amain(const char* host, short port) {
    try {
        std::array<char, 1024> buf;

        auto client = co_await TcpClient::create();
        auto conn = co_await client.connect(host, port);

        for (int i = 0; i < 5; ++i) {
            co_await conn.send("Hello server", 12);
            size_t recv_len = co_await conn.receive(buf.data(), buf.size());
            cout << string_view(buf.data(), recv_len) << '\n';
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain("127.0.0.1", 8080), detached);
    loop.run();
}