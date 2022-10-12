#include <iostream>
#include "magio/magio.h"

using namespace std;
using namespace magio;

Coro<void> process(TcpStream stream) {
    try {
        char buf[1024];
        for (; ;) {
            auto rdlen = co_await stream.read(buf, sizeof(buf));
            co_await stream.write(buf, rdlen);
        }
    } catch (const runtime_error& e) {
        // cout << e.what() << '\n';
    }
}

Coro<void> amain(Magico& loop) {
    try {
        auto server = co_await TcpServer::bind("127.0.0.1", 8000);
        auto exe = co_await this_coro::executor;
        for (; ;) {
            auto stream = co_await server.accept();
            co_spawn(exe, process(std::move(stream)));
        }
    } catch (const runtime_error& e) {
        cout << e.what() << '\n';
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain(loop));
    loop.run();
}