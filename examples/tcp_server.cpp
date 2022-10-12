#include <iostream>
#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operation;

Coro<> process(TcpStream stream) {
    try {
        array<char, 1024> buf;
        for (; ;) {
            auto [rdlen, _] = co_await (
                stream.read(buf.data(), buf.size()) &&
                stream.write("Hello client..", 14)
            );
            cout << string_view(buf.data(), rdlen) << '\n';
        }
    } catch(const std::runtime_error& err) {
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
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}