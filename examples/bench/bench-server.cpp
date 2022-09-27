#include "magio/Runtime.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/ThisCoro.h"
#include "magio/tcp/Tcp.h"

using namespace std;
using namespace magio;

Coro<void> process(TcpStream stream) {
    for (; ;) {
        auto [buf, rdlen] = co_await stream.vread();
        co_await stream.write(buf, rdlen);
    }
}

Coro<void> amain() {
    auto exe = co_await this_coro::executor;
    auto server = co_await TcpServer::bind("127.0.0.1", 8000);
    for (; ;) {
        auto stream = co_await server.accept();
        co_spawn(exe, process(std::move(stream)), detached);
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain(), detached);
    loop.run();
}