#include <array>
#include <iostream>
#include "magio/Runtime.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/tcp/Tcp.h"

using namespace std;
using namespace magio;

Coro<void> amain() {
    try {
        std::array<char, 1024> buf;
        auto client = co_await TcpClient::create();
        auto stream = co_await client.connect("127.0.0.1", 1234, "127.0.0.1", 8080);

        for (int i = 0; i < 5; ++i) {
            co_await stream.write("Hello server..", 14);
            size_t recv_len = co_await stream.read(buf.data(), buf.size());

            cout << string_view(buf.data(), recv_len) << '\n';
        }
    } catch(const std::runtime_error& err) {
        cout <<  err.what() << '\n';
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}