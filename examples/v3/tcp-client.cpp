#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

Coro<> client() {
    auto local = net::InetAddress::from("::1", 0) | throw_on_err;
    auto peer = net::InetAddress::from("::1", 1234) | throw_on_err;
    auto socket = net::Socket::open(net::Ip::v6, net::Transport::Tcp) | throw_on_err;

    socket.bind(local) | throw_on_err;
    co_await socket.connect(peer) | throw_on_err;
 
    char buf[1024];
    for (int i = 0; i < 5; ++i) {
        co_await socket.send("hello server", 12) | throw_on_err;
        size_t rd = co_await socket.receive(buf, sizeof(buf)) | throw_on_err;
        if (rd == 0) {
            M_ERROR("{}", "EOF");
            break;
        }
        M_INFO("{}", string_view(buf, rd));
    }
}

int main() {
    CoroContext ctx(128);
    this_context::spawn(client(), [](exception_ptr eptr, Unit) {
        try {
            try_rethrow(eptr);
        } catch(const system_error& err) {
            M_ERROR("{}", err.what());
        }
        this_context::stop();
    });
    ctx.start();
}