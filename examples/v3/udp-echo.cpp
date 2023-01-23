#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> amain() {
    net::EndPoint local(net::make_address("::1") | throw_err, 1234);
    auto socket = net::Socket::open(net::Ip::v6, net::Transport::Udp) | throw_err;
    socket.bind(local) | panic_on_err;

    char buf[1024];
    for (; ;) {
        auto [rd, peer] = co_await socket.receive_from(buf, sizeof(buf)) | throw_err;
        M_INFO("[{}]:{}: {}", peer.address().to_string(), peer.port(), string_view(buf, rd));
        co_await socket.send_to(buf, rd, peer) | throw_err;
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(amain(), [](exception_ptr eptr, Unit) {
        try {
            try_rethrow(eptr);
        } catch(const std::system_error& err) {
            M_ERROR("{}", err.what());
        }
        this_context::stop();
    });
    ctx.start();    
}