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

template<typename Rep, typename Per>
void ticker(const chrono::duration<Rep, Per>& duration, size_t times, std::function<void()>&& func) {
    this_context::expires_after(duration, [duration, times, func = std::move(func)](bool flag) mutable {
        if (!flag) {
            return;
        }
        func();
        --times;
        if (times != 0) {
            ticker(duration, times, std::move(func));
        } else {
            this_context::stop();
        }
    });
}


int main() {
    CoroContext ctx(128);
    ticker(2s, 999, [] {
        M_INFO("once");
    });
    this_context::spawn(server());
    ctx.start();
}