#include <array>
#include "magio/magio.h"

using namespace std;
using namespace magio;
using namespace magio::operators;

Coro<> process(TcpStream stream) {
    try {
        array<char, 1024> buf;
        for (; ;) {
            auto [rdlen, _] = co_await (
                stream.read(buf.data(), buf.size()) &&
                stream.write("Hello client..", 14)
            );

            M_INFO("{}", string_view(buf.data(), rdlen));
        }
    } catch(const system_error& err) {
        M_WARN("{}", err.what());
    }
}

Coro<> amain() {
    try {
        auto server = co_await TcpServer::bind("0.0.0.0", 8000);
        for (; ;) {
            auto stream = co_await server.accept();

            M_INFO("{} connect {}", stream.local_address().to_string(), stream.remote_address().to_string());

            co_spawn(co_await this_coro::executor, process(std::move(stream)));
        }
    } catch(const runtime_error& err) {
        M_ERROR("{}", err.what());
    }
}

int main() {
    Magico loop(1);
    co_spawn(loop.get_executor(), amain());
    loop.run();
}