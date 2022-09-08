#include "magio/tcp/TcpServer.h"

#include "magio/plat/iocp.h"
#include "magio/plat/socket.h"
#include "magio/ServerConfig.h"
#include "magio/coro/Coro.h"

namespace magio {
/*
struct TcpServer::Impl {
    plat::IocpServer server;
};

struct TcpStream::Impl {
    plat::IocpServer* server_;
    plat::SocketHelper sock_;

    struct RWHook {
        Error err;
        std::coroutine_handle<> h;
    } hook;

    plat::CompletionHandler completion_handler;
    
    ~Impl() {
        server_->repost_accept_task(sock_);
    }
};

CLASS_PIMPL_IMPLEMENT(TcpServer)

Coro<TcpServer> TcpServer::bind(const char* host, short port) {
    if (auto res = plat::IocpServer::bind(host, port)) {
        auto impl = new Impl{res.unwrap()};

        for (size_t i = 0; i < server_config.listener_num; ++i) {
            impl->server.post_accept_task(nullptr);
        }

        auto executor = co_await this_coro::executor;
        executor.post([] {
            
        });

        co_return {impl};
    } else {
        throw std::runtime_error(res.unwrap_err().msg);
    }
}

Coro<TcpStream> TcpServer::accept() {
    struct AcceptHook {
        Error err;
        plat::SocketHelper socket{nullptr};
        std::coroutine_handle<> h;
    } hook;

    plat::CompletionHandler handler{
        (void*)&hook, 
        [](void* hook, Error err, plat::SocketHelper sock) {
            auto ahook = (AcceptHook*)hook;
            if (err) {
                ahook->err = err;
            }
            ahook->socket = sock;
            ahook->h.resume();
        }
    };

    co_await CallbackAwaiter{
        [&](std::coroutine_handle<> h) {
            hook.h = h;
            impl->server.add_listener(&handler);
        }};

    if (hook.err) {
        co_return hook.err;
    }


    auto tcp_impl = new TcpStream::Impl{&impl->server, hook.socket};

    co_return TcpStream{tcp_impl};
}

//

CLASS_PIMPL_IMPLEMENT(TcpStream)

Coro<size_t> TcpStream::read(char* buf, size_t len) {
    impl->hook.err = Error();
    impl->completion_handler = plat::CompletionHandler{
        (void*)&impl->hook, 
        [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (Impl::RWHook*)hook;
            if (err) {
                rwhook->err = err;
            }
            rwhook->h.resume();
        }
    };

    co_await CallbackAwaiter{
        [&](std::coroutine_handle<> h) {
            impl->hook.h = h;
            impl->server_->post_receive_task(impl->sock_, &impl->completion_handler);
        }};

    
    if (impl->hook.err) {
        co_return impl->hook.err;
    }

    size_t ret_len = impl->sock_.recv_io().len() > len ? len : impl->sock_.recv_io().len(); 
    std::memcpy(buf, impl->sock_.recv_io().buf(), ret_len);

    co_return ret_len;
}

Coro<size_t> TcpStream::write(const char* buf, size_t len) {
    impl->hook.err = Error();
    impl->completion_handler = plat::CompletionHandler{
        (void*)&impl->hook, 
        [](void* hook, Error err, plat::SocketHelper sock) {
            auto whook = (Impl::RWHook*)hook;
            if (err) {
                whook->err = err;
            }
            whook->h.resume();
        }
    };
    
    size_t cp_len = impl->sock_.send_io().capacity() > len ? len : impl->sock_.send_io().capacity();
    std::memcpy(impl->sock_.send_io().buf(), buf, cp_len);
    impl->sock_.send_io().set_len(cp_len);

    // LOG("{}\n", std::string_view(impl->sock_.send_io().buf(), cp_len));

    co_await CallbackAwaiter{
        [&](std::coroutine_handle<> h) {
            impl->hook.h = h;
            impl->server_->post_send_task(impl->sock_);
        }};

    if (impl->hook.err) {
        co_return impl->hook.err;
    }
    co_return impl->sock_.send_io().len();
}
*/
}