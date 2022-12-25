#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operators;

Coro<> amain() {
    try {
        auto stream = co_await TcpStream::connect("127.0.0.1", 8000);

        M_INFO("{} connect {}", stream.local_address().to_string(), stream.remote_address().to_string());

        array<char, 1024> buf;
        for (int i = 0; i < 5; ++i) {
            auto [_, rdlen] = co_await (
                stream.write("Hello server..", 14) &&
                stream.read(buf.data(), buf.size())
            );

            M_INFO("{}", string_view(buf.data(), rdlen));
        }
    } catch(const std::exception& err) {
        M_ERROR("{}", err.what());
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}