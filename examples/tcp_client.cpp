#include <iostream>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operation;

Coro<void> amain() {
    try {
        auto client = co_await TcpClient::create();
        auto stream = co_await client.connect("127.0.0.1", 8000);

        cout << stream.remote_address().to_string()
             << " connect "
             << stream.local_address().to_string()
             << '\n';

        for (int i = 0; i < 5; ++i) {
            auto [wlen, str] = co_await (
                stream.write("Hello server..") | 
                stream.read()
            );
            cout << str << '\n';
        }
    } catch(const std::exception& err) {
        cout <<  err.what() << '\n';
    }
}

int main() {
    this_context::run(amain());
}