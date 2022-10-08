#include <iostream>
#include "magio/magio.h"

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
            this_context::spawn(process(std::move(stream)));
        }
    } catch (const runtime_error& e) {
        cout << e.what() << '\n';
    }
}

int main() {
    this_context::run(amain());
}