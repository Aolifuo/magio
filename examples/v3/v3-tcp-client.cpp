#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> client() {
    std::error_code ec;
    net::EndPoint local(net::make_address("::1", ec), 0);
    net::EndPoint peer(net::make_address("::1", ec), 1234);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    net::Socket socket;
    socket.open(net::Ip::v6, net::Transport::Tcp, ec);
    socket.bind(local, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    co_await socket.connect(peer, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }
 
    char buf[1024];
    for (int i = 0; i < 5; ++i) {
        co_await socket.write("hello server", 12, ec);
        if (ec) {
            M_ERROR("{}", ec.message());
            break;
        }
        size_t rd = co_await socket.read(buf, sizeof(buf), ec);
        if (ec) {
            M_ERROR("{}", ec.message());
            break;
        }
        M_INFO("{}", string_view(buf, rd));
    }
    this_context::stop();
}

int main() {
    CoroContext ctx(100);
    this_context::spawn(client());
    ctx.start();
}