#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace operation;

Coro<> amain(EventLoop& loop) {
    try {
        array<char, 1024> buf;
        auto socket = co_await UdpSocket::bind("127.0.0.1", 8001);
        for (size_t i = 0; i < 5; ++i) {
            auto [_, pair] = co_await (
                socket.write_to("Hello", 5, {"127.0.0.1", 8002}) &&
                socket.read_from(buf.data(), buf.size())
            );
            cout << "recv from " << pair.second.to_string() << ":" << pair.first << '\n';
        }
    } catch(const runtime_error& e) {
        cout << e.what() << '\n';
    }
    
    loop.stop();
}

int main() {
    EventLoop loop(1);
    co_spawn(loop.get_executor(), amain(loop));
    loop.run();
}