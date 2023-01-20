#include "magio-v3/magio.h"

using namespace std;
using namespace magio;
using namespace chrono_literals;

class TcpServer {
public:
    TcpServer(CoroContextPool& pool): pool_(pool)
    { }

    void start() {
        this_context::spawn(server());
    }

private:
    Coro<> server() {
        error_code ec;
        net::EndPoint local(net::make_address("::1", ec), 1234);
        if (ec) {
            M_FATAL("{}", ec.message());
        }

        acceptor_ = net::Acceptor::listen(local, ec);
        if (ec) {
            M_FATAL("{}", ec.message());
        }

        for (size_t i = 0; i < 10; ++i) {
            this_context::spawn(accept());
        }
        
        co_return;
    }

    Coro<> accept() {
        for (; ;) {
            error_code ec;
            auto [socket, peer] = co_await acceptor_.accept(ec);
            if (ec) {
                M_ERROR("accept error: {}", ec.message());
            } else {
                M_INFO("accept [{}]:{}", peer.address().to_string(), peer.port());
                pool_.next_context().spawn(handle_connection(std::move(socket)));
            }
        }
    }

    Coro<> handle_connection(net::Socket sock) {
        char buf[1024];
        for (; ;) {
            error_code ec;
            size_t rd = co_await sock.receive(buf, sizeof(buf), ec);
            if (ec || rd == 0) {
                M_ERROR("recv error: {}", ec ? ec.message() : "EOF");
                break;
            }
            M_INFO("{}", string_view(buf, rd));
            co_await sock.send(buf, rd, ec);
            if (ec) {
                M_ERROR("send error: {}", ec.message());
                break;
            }
        }
    }

    CoroContextPool& pool_;
    net::Acceptor acceptor_;
};

int main() {
    CoroContextPool ctx_pool(4, 50);
    TcpServer server(ctx_pool);
    server.start();
    ctx_pool.start_all();
}