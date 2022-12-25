#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> handle_connection(net::Socket sock) {
    std::error_code ec;
    char buf[1024];
    for (; ;) {
        size_t rd = co_await sock.read(buf, sizeof(buf), ec);
        if (ec || rd == 0) {
            M_ERROR("{}", ec ? ec.message() : "EOF");
            break;
        }
        M_INFO("receive: {}", string_view(buf, rd));
        co_await sock.write(buf, rd, ec);
        if (ec) {
            M_ERROR("{}", ec.message());
            break;
        }
    }
}

Coro<> server() {
    std::error_code ec;
    net::EndPoint local(net::make_address("::1", ec), 1234);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    net::Acceptor acceptor;
    acceptor.bind_and_listen(local, net::Transport::Tcp, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    for (; ;) {
        auto [socket, peer] = co_await acceptor.accept(ec);
        if (ec) {
            M_ERROR("{}", ec.message());
        }
        M_INFO("accept [{}]:{}", peer.address().to_string(), peer.port());
        this_context::spawn(handle_connection(std::move(socket)));
    }
}

int main() {
    CoroContext ctx(true);
    this_context::spawn(server());
    ctx.start();
}