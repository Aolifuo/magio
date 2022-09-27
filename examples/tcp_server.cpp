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
        for (; ;) {
            auto [buf, rdlen] = co_await stream.vread();
            cout << string_view(buf, rdlen) << '\n';
            co_await stream.write(buf, rdlen);
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

            cout << stream.remote_address().to_string() 
                 << " connect "
                 << stream.local_address().to_string()
                 << '\n';
        
            co_spawn(executor, process(std::move(stream)), detached);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain("127.0.0.1", 8000), detached);
    loop.run();
}