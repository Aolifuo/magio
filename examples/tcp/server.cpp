#include <iostream>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operation;

constexpr size_t CONNS = 5;

Coro<void> process(TcpStream stream) {
    try {
        for (; ;) {
            auto str = co_await stream.read();
            cout << str << '\n';
            co_await stream.write(str);
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

Coro<void> amain() {
    try {
        auto server = co_await TcpServer::bind("127.0.0.1", 8000);
        for (size_t i = 0; i < CONNS; ++i) {
            auto stream = co_await server.accept();

            // cout << stream.remote_address().to_string() 
            //      << " connect "
            //      << stream.local_address().to_string()
            //      << '\n';

            if (i == CONNS - 1) {
                co_await this_context::spawn(process(std::move(stream)), use_coro);
            } else {
                this_context::spawn(process(std::move(stream)));
            }
            
        }
    } catch(const std::runtime_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    this_context::run(amain());
}