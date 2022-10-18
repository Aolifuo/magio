#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;

Coro<> amain() {
    try {
        array<char, 1024> buf;
        auto socket = co_await UdpSocket::bind("127.0.0.1", 8001);
        for (; ;) {
            // 5秒后超时取消read
            auto read_res = co_await timeout(5000, socket.read_from(buf.data(), buf.size()));
            if (!read_res) {
                cout << "read timeout!\n";
                continue;
            }

            auto [len, address] = read_res.unwrap();
            cout << string_view(buf.data(), len) << '\n';

            auto write_res = co_await timeout(5000, socket.write_to(buf.data(), len, address));
            if (!write_res) {
                cout << "write timeout!\n";
                continue;
            }
        }
    } catch (const system_error& err) {
        cout << err.what() << '\n';
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}