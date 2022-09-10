#include <array>
#include <iostream>
#include "magio/Runtime.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/ThisCoro.h"
#include "magio/tcp/Tcp.h"

using namespace std;
using namespace magio;

Coro<void> process(TcpStream stream) {
    try {
        array<char, 1024> buf;
        for (; ;) {
            size_t read_len = co_await stream.read(buf.data(), buf.size());
            cout << string_view(buf.data(), read_len) << '\n';
            co_await stream.write("Hello client..", 14);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

Coro<void> amain(const char* host, short port) {
    try {
        auto executor = co_await this_coro::executor;
        auto server = co_await TcpServer::bind(host, port);
        for (; ;) {
            auto stream = co_await server.accept();
            co_spawn(executor, process(std::move(stream)), detached);
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