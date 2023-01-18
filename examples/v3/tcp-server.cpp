#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> handle_connection(net::Socket sock) {
    char buf[1024];
    for (; ;) {
        error_code ec;
        size_t rd = co_await sock.receive(buf, sizeof(buf), ec);
        if (ec || rd == 0) {
            M_ERROR("recv error: {}", ec ? ec.message() : "EOF");
            break;
        }
        M_INFO("receive: {}", string_view(buf, rd));
        co_await sock.send(buf, rd, ec);
        if (ec) {
            M_ERROR("send error: {}", ec.message());
            break;
        }
    }
}

Coro<> server() {
    error_code ec;
    net::EndPoint local(net::make_address("::1", ec), 1234);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    auto acceptor = net::Acceptor::bind_and_listen(local, ec);
    if (ec) {
        M_FATAL("{}", ec.message());
    }

    for (; ; ec.clear()) {
        auto [socket, peer] = co_await acceptor.accept(ec);
        if (ec) {
            M_ERROR("{}", ec.message());
        } else {
            M_INFO("accept [{}]:{}", peer.address().to_string(), peer.port());
            this_context::spawn(handle_connection(std::move(socket)));
        }
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(server());
    ctx.start();
}