#include <array>
#include "magio/Runtime.h"
#include "magio/EventLoop.h"
#include "magio/coro/CoSpawn.h"
#include "magio/coro/ThisCoro.h"
#include "magio/tcp/TcpServer.h"

using namespace std;
using namespace magio;

const string resp = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html;charset=UTF-8\r\n"
    "Content-Length: 25\r\n"
    "\r\n"
    "<html>Hello world</html>";

Coro<void> process(TcpStream stream) {
    try {
        array<char, 1024> buf;
        co_await stream.read(buf.data(), buf.size());
        co_await stream.write(resp.data(), resp.length());
    } catch(const std::runtime_error& err) {
        printf("%s\n", err.what());
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
        printf("%s\n", err.what());
    }
}

int main() {
    Runtime::run().unwrap();
    EventLoop loop;
    co_spawn(loop.get_executor(), amain("127.0.0.1", 8080), detached);
    loop.run();
}