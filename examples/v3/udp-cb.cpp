#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

class UdpEcho {
public:
    void start(const char* ip, net::PortType port) {
        net::EndPoint ep(net::make_address(ip) | panic_on_err, 1234);
        socket_ = net::Socket::open(net::Ip::v6, net::Transport::Udp) | panic_on_err;
        socket_.bind(ep) | panic_on_err;

        receive_from();
    }

    void stop() {
        this_context::stop();
    }

private:
    void receive_from() {
        socket_.receive_from(buf, sizeof(buf), [&](error_code ec, size_t len, net::EndPoint ep) {
            if (ec) {
                M_ERROR("recv error: {}", ec.message());
                return stop();
            }
            M_INFO("{}", string_view(buf, len));
            send_to(string_view(buf, len), ep);
        });
    }

    void send_to(string_view msg, const net::EndPoint& ep) {
        socket_.send_to(msg.data(), msg.length(), ep, [&](error_code ec, size_t len) {
            if (ec) {
                M_ERROR("send error: {}", ec.message());
                return stop();
            }
            receive_from();
        });
    }
    
    char buf[1024];
    net::Socket socket_;
};

int main() {
    CoroContext ctx(128);
    UdpEcho echo;
    echo.start("::1", 1234);
    ctx.start();
}