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
        net::EndPoint local(net::make_address("::1") | panic_on_err, 1234);
        acceptor_ = net::Acceptor::listen(local) | panic_on_err;

        for (size_t i = 0; i < 10; ++i) {
            this_context::spawn(accept());
        }
        
        co_return;
    }

    Coro<> accept() {
        for (; ;) {
            error_code ec;
            auto [socket, peer] = co_await acceptor_.accept() | redirect_err(ec);
            if (ec) {
                M_ERROR("accept error: {}", ec.message());
            } else {
                M_INFO("accept [{}]:{}", peer.address().to_string(), peer.port());
                pool_.next_context().spawn(handle_conn(std::move(socket)), [](exception_ptr eptr, Unit) {
                    try {
                        try_rethrow(eptr);
                    } catch(const system_error& err) {
                        M_ERROR("{}", err.what());
                    }
                });
            }
        }
    }

    Coro<> handle_conn(net::Socket sock) {
        char buf[1024];
        for (; ;) {
            size_t rd = co_await sock.receive(buf, sizeof(buf)) | throw_on_err;
            if (rd == 0) {
                M_ERROR("{}", "EOF");
                break;
            }
            M_INFO("{}", string_view(buf, rd));
            co_await sock.send(buf, rd) | throw_on_err;
        }
    }

    CoroContextPool& pool_;
    net::Acceptor acceptor_;
};

int main() {
    Logger::set_output(AsyncLogger::write);
    CoroContextPool ctx_pool(4, 50);
    TcpServer server(ctx_pool);
    server.start();
    ctx_pool.start_all();
}