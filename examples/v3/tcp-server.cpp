#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> handle_conn(net::Socket sock) {
    char buf[1024];
    for (; ;) {
        size_t rd = co_await sock.receive(buf, sizeof(buf)) | throw_on_err;
        if (rd == 0) {
            M_INFO("{}", "EOF");
            break;
        }
        M_INFO("receive: {}", string_view(buf, rd));
        co_await sock.send(buf, rd) | throw_on_err;
    }
}

Coro<> server() {
    net::EndPoint local(net::make_address("::1") | panic_on_err, 1234);
    auto acceptor = net::Acceptor::listen(local) | panic_on_err;

    for (; ;) {
        error_code ec;
        auto [socket, peer] = co_await acceptor.accept() | redirect_err(ec);
        if (ec) {
            M_ERROR("{}", ec.message());
        } else {
            M_INFO("accept [{}]:{}", peer.address().to_string(), peer.port());
            this_context::spawn(handle_conn(std::move(socket)), [](exception_ptr eptr, Unit) {
                try {
                    try_rethrow(eptr);
                } catch(const std::system_error& err) {
                    M_ERROR("{}", err.what());
                }
            });
        }
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(server());
    ctx.start();
}