#include <iostream>
#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operation;

// constexpr size_t CONNS = 3;

Coro<> process(TcpStream stream) {
    try {
        array<char, 1024> buf;
        for (; ;) {
            auto rdlen = co_await stream.read(buf.data(), buf.size());
            cout << string_view(buf.data(), rdlen) << '\n';
            co_await stream.write(buf.data(), rdlen);
        }
    } catch(const runtime_error& err) {
        cout << err.what() << '\n';
    }
}

Coro<> amain() {
    try {
        auto server = co_await TcpServer::bind("127.0.0.1", 8000);
        for (; ;) {
            auto stream = co_await server.accept();

            cout << stream.local_address().to_string() 
                 << " connect "
                 << stream.remote_address().to_string()
                 << '\n';

            co_spawn(co_await this_coro::executor, process(std::move(stream)));
        }
    } catch(const runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    try {
        Magico loop(1);
        co_spawn(loop.get_executor(), amain());
        loop.run();
    } catch(const runtime_error& err) {
        cout << err.what() << '\n';
    }
}