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
                M_WARN("read timeout!");
                continue;
            }

            auto [len, address] = read_res.unwrap();
            M_INFO("{}", string_view(buf.data(), len));

            auto write_res = co_await timeout(5000, socket.write_to(buf.data(), len, address));
            if (!write_res) {
                M_WARN("read timeout!");
                continue;
            }
        }
    } catch (const system_error& err) {
        M_ERROR("{}", err.what());
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}