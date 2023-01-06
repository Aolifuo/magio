#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

class UdpEcho {
public:
    void start(const char* ip, net::PortType port) {
        error_code ec;
        net::EndPoint ep(net::make_address(ip, ec), 1234);
        socket_.open(net::Ip::v6, net::Transport::Udp, ec);
        socket_.bind(ep, ec);
        if (ec) {
            M_FATAL("{}", ec.message());
        }

        receive_from();
    }

private:
    void receive_from() {
        socket_.receive_from(buf, sizeof(buf), [&](error_code ec, size_t len, net::EndPoint ep) {
            if (ec) {
                M_ERROR("recv error: {}", ec.message());
                return receive_from();
            }
            M_INFO("{}", string_view(buf, len));
            send_to(string_view(buf, len), ep);
        });
    }

    void send_to(string_view msg, const net::EndPoint& ep) {
        socket_.send_to(msg.data(), msg.length(), ep, [&](error_code ec, size_t len) {
            if (ec) {
                M_ERROR("send error: {}", ec.message());
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