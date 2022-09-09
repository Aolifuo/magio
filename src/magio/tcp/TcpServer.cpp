#include "magio/tcp/TcpServer.h"

#include "magio/coro/Coro.h"
#include "magio/coro/ThisCoro.h"
#include "magio/plat/iocp.h"
#include "magio/plat/socket.h"
#include "magio/Configs.h"
#include <winerror.h>

namespace magio {

struct TcpServer::Impl {
    plat::IocpServer server;
};

struct TcpStream::Impl {
    plat::IocpServer* server_;
    plat::SocketHelper sock_;

    struct RWHook {
        Error err;
        std::coroutine_handle<> h;
        AnyExecutor exe;
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
        executor.waiting([impl]() ->bool {
            int status = impl->server.wait_completion_task();
            if (status == ERROR_ABANDONED_WAIT_0 
                || status == ERROR_INVALID_HANDLE) 
            {
                return true;
            }

            return false;
        });

        co_return {impl};
    } else {
        throw std::runtime_error(res.unwrap_err().msg);
    }
}

Coro<TcpStream> TcpServer::accept() {
    auto executor = co_await this_coro::executor;

    struct AcceptHook {
        Error err;
        plat::SocketHelper socket{nullptr};
        std::coroutine_handle<> h;
        AnyExecutor exe;
    } hook{.exe = executor};

    plat::CompletionHandler handler{
        (void*)&hook, 
        [](void* hook, Error err, plat::SocketHelper sock) {
            auto ahook = (AcceptHook*)hook;
            if (err) {
                ahook->err = err;
            }
            ahook->socket = sock;
            ahook->exe.post([=] { ahook->h.resume(); });
        }
    };

    co_await Coro<void>{
        [&hook, &handler, impl = impl](std::coroutine_handle<> h) {
            hook.h = h;
            impl->server.add_listener(&handler);
        }
    };

    if (hook.err) {
        throw std::runtime_error(hook.err.msg);
    }

    auto tcp_impl = new TcpStream::Impl{&impl->server, hook.socket};
    co_return TcpStream{tcp_impl};
}

//

CLASS_PIMPL_IMPLEMENT(TcpStream)

Coro<size_t> TcpStream::read(char* buf, size_t len) {
    auto executor = co_await this_coro::executor;
    impl->hook.exe = executor;
    impl->hook.err = Error();
    impl->completion_handler = plat::CompletionHandler{
        (void*)&impl->hook, 
        [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (Impl::RWHook*)hook;
            if (err) {
                rwhook->err = err;
            }
            rwhook->exe.post([=] { rwhook->h.resume(); });
        }
    };

    co_await Coro<void>{
        [impl = impl](std::coroutine_handle<> h) {
            impl->hook.h = h;
            impl->server_->post_receive_task(impl->sock_, &impl->completion_handler);
        }
    };
    
    if (impl->hook.err) {
        throw std::runtime_error(impl->hook.err.msg);
    }

    size_t ret_len = impl->sock_.recv_io().len() > len ? len : impl->sock_.recv_io().len(); 
    std::memcpy(buf, impl->sock_.recv_io().buf(), ret_len);

    co_return ret_len;
}

Coro<size_t> TcpStream::write(const char* buf, size_t len) {
    auto executor = co_await this_coro::executor;
    impl->hook.exe = executor;
    impl->hook.err = Error();
    impl->completion_handler = plat::CompletionHandler{
        (void*)&impl->hook, 
        [](void* hook, Error err, plat::SocketHelper sock) {
            auto whook = (Impl::RWHook*)hook;
            if (err) {
                whook->err = err;
            }
            whook->exe.post([=] { whook->h.resume(); });
        }
    };
    
    size_t cp_len = impl->sock_.send_io().capacity() > len ? len : impl->sock_.send_io().capacity();
    std::memcpy(impl->sock_.send_io().buf(), buf, cp_len);
    impl->sock_.send_io().set_len(cp_len);

    // LOG("{}\n", std::string_view(impl->sock_.send_io().buf(), cp_len));

    co_await Coro<void>{
        [impl = impl](std::coroutine_handle<> h) {
            impl->hook.h = h;
            impl->server_->post_send_task(impl->sock_);
        }
    };

    if (impl->hook.err) {
        throw std::runtime_error(impl->hook.err.msg);
    }
    
    co_return impl->sock_.send_io().len();
}

}