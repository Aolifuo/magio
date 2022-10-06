#include <array>
#include <iostream>
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/net/tcp/Tcp.h"
#include "magio/coro/Operation.h"

using namespace std;
using namespace magio;
using namespace magio::operation;

Coro<void> process(TcpStream stream) {
    try {
        for (; ;) {
            auto [str, wlen] = co_await (
                stream.read() | 
                stream.write("hello client", 12)
            );
            cout << str << '\n';
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

Coro<void> amain() {
    try {
        auto server = co_await TcpServer::bind("127.0.0.1", 8000);
        for (; ;) {
            auto stream = co_await server.accept();

            cout << stream.remote_address().to_string() 
                 << " connect "
                 << stream.local_address().to_string()
                 << '\n';
        
            co_spawn(co_await this_coro::executor, process(std::move(stream)), detached);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}