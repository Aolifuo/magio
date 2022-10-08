#include <iostream>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operation;

Coro<void> process(TcpStream stream) {
    try {
        for (; ;) {
            auto [str, wlen] = co_await (
                stream.read() |
                stream.write("Hello client..")
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

            this_context::spawn(process(std::move(stream)));
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    this_context::run(amain());
}