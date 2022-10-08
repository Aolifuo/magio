#include <iostream>
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/net/tcp/Tcp.h"

using namespace std;
using namespace magio;

Coro<void> process(TcpStream stream) {
    for (; ;) {
        auto str = co_await stream.read();
        co_await stream.write(str);
    }
}

Coro<void> amain() {
    try {
        auto server = co_await TcpServer::bind("127.0.0.1", 8000);
        for (; ;) {
            auto stream = co_await server.accept();
            co_spawn(co_await this_coro::executor, process(std::move(stream)), detached);
        }
    } catch (const runtime_error& e) {
        cout << e.what() << '\n';
    }
    
}

int main() {
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}