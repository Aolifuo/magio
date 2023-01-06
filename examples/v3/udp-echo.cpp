#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> amain() {
    error_code ec;
    net::Socket socket;
    socket.open(net::Ip::v6, net::Transport::Udp, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    net::EndPoint local(net::make_address("::1", ec), 1234);
    socket.bind(local, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    char buf[1024]{};
    for (; ; ec.clear()) {
        auto [rd, peer] = co_await socket.receive_from(buf, sizeof(buf), ec);
        if (ec) {
            M_ERROR("recv error: {}", ec.message());
            continue;
        }
        M_INFO("[{}]:{}: {}", peer.address().to_string(), peer.port(), string_view(buf, rd));
        co_await socket.send_to(buf, rd, peer, ec);
        if (ec) {
            M_ERROR("send error: {}", ec.message());
            continue;
        }
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(amain());
    ctx.start();    
}