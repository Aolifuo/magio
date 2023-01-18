#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

class TcpConnection: public enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(net::Socket socket)   
        : socket_(std::move(socket)) { }

    void start() {
        receive();
    }

private:
    void receive() {
        socket_.receive(buf_, sizeof(buf_), [p = shared_from_this()](error_code ec, size_t len) {
            if (ec || len == 0) {
                M_ERROR("receive error: {}", ec ? ec.message() : "EOF");
                return;
            }
            M_INFO("{}", string_view(p->buf_, len));
            p->send(string_view(p->buf_, len));
        });
    }

    void send(string_view msg) {
        socket_.send(msg.data(), msg.length(), [p = shared_from_this()](error_code ec, size_t len) {
            if (ec) {
                M_ERROR("send error: {}", ec.value());
                return;
            }
            p->receive();
        });
    }

    net::Socket socket_;
    char buf_[1024];
};

class TcpServer {
public:
    void start(const char* ip, net::PortType port) {
        error_code ec;
        net::EndPoint ep(net::make_address(ip, ec), port);
        if (ec) {
            M_FATAL("cannot make address: {}", ec.message());
        }
        acceptor_ = net::Acceptor::bind_and_listen(ep, ec);
        if (ec) {
            M_FATAL("cannot bind and listen: {}", ec.message());
        }
        accept();
    }

private:
    void accept() {
        acceptor_.accept([&](error_code ec, net::Socket socket, net::EndPoint ep) {
            if (ec) {
                M_ERROR("accept error {}", ec.message());
            } else {
                M_INFO("accept [{}]:{}", ep.address().to_string(), ep.port());
                auto conn = make_shared<TcpConnection>(std::move(socket));
                conn->start();
            }
            accept();
        });
    }

    net::Acceptor acceptor_;
};

int main() {
    CoroContext ctx(128);
    TcpServer server;
    server.start("::1", 1234);
    ctx.start();
}