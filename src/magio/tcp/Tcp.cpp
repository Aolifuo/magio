#include "magio/tcp/Tcp.h"

#include "magio/Configs.h"
#include "magio/coro/Coro.h"
#include "magio/plat/iocp.h"
#include "magio/plat/socket.h"
#include "magio/plat/system_errors.h"

namespace magio {

struct RWHook {
    Error err;
    plat::SocketHelper sock{nullptr};
    std::coroutine_handle<> h;
    AnyExecutor exe;
};

// TcpStream

struct TcpStream::Impl {
    plat::IocpInterface* iocp_;
    RWHook hook_;
    plat::CompletionHandler* handler_;
    
    Impl(AnyExecutor exe, plat::IocpInterface* iocp, RWHook hook, plat::CompletionHandler* handler)
        : iocp_(iocp), hook_(hook), handler_(handler)
    {
        hook.exe = exe;
        handler->hook = (void*)&hook_;
        handler->cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (RWHook*)hook;
            rwhook->err = err;
            rwhook->sock = sock;
            rwhook->exe.post([h = rwhook->h] { h.resume(); });
        };
    }

    ~Impl() {
        iocp_->recycle(hook_.sock);
        delete handler_;
    }
};

CLASS_PIMPL_IMPLEMENT(TcpStream)

Coro<size_t> TcpStream::read(char *buf, size_t len) {
    impl->hook_.err = Error();

    co_await Coro<void>{
        [impl = impl](std::coroutine_handle<> h) {
            impl->hook_.h = h;
            if (auto res = impl->iocp_->post_receive_task(impl->hook_.sock); !res) {
                impl->hook_.err = res.unwrap_err();
                h.resume();
            }
        }
    };

    if (impl->hook_.err) {
        throw std::runtime_error(impl->hook_.err.msg);
    }

    size_t ret_len = impl->hook_.sock.recv_io().len() > len ? len : impl->hook_.sock.recv_io().len(); 
    std::memcpy(buf, impl->hook_.sock.recv_io().buf(), ret_len);

    co_return ret_len;
}

Coro<size_t> TcpStream::write(const char *buf, size_t len) {
    impl->hook_.err = Error();

    size_t cp_len = impl->hook_.sock.send_io().capacity() > len ? len : impl->hook_.sock.send_io().capacity();
    std::memcpy(impl->hook_.sock.send_io().buf(), buf, cp_len);
    impl->hook_.sock.send_io().set_len(cp_len);

    co_await Coro<void>{
        [impl = impl](std::coroutine_handle<> h) {
            impl->hook_.h = h;
            if (auto res = impl->iocp_->post_send_task(impl->hook_.sock); !res) {
                impl->hook_.err = res.unwrap_err();
                h.resume();
            }
        }
    };

    if (impl->hook_.err) {
        throw std::runtime_error(impl->hook_.err.msg);
    }

    co_return impl->hook_.sock.send_io().len();
}

// TcpServer

struct TcpServer::Impl {
    plat::IocpServer server;
};

CLASS_PIMPL_IMPLEMENT(TcpServer)

Coro<TcpServer> TcpServer::bind(const char* host, short port) {
    if (auto res = plat::IocpServer::bind(host, port)) {
        auto impl = new Impl{res.unwrap()};

        auto executor = co_await this_coro::executor;
        executor.waiting([impl]() ->bool {
            int status = impl->server.wait_completion_task();
            if (status == ERROR_ABANDONED_WAIT_0 
                || status == ERROR_INVALID_HANDLE
            ) {
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

    RWHook hook{.exe = executor};

    auto handler = new plat::CompletionHandler{
        .hook = (void*)&hook, 
        .cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (RWHook*)hook;
            rwhook->err = err;
            rwhook->sock = sock;
            rwhook->exe.post([h = rwhook->h] { h.resume(); });
        }
    };

    co_await Coro<void>{
        [&hook, handler, impl = impl](std::coroutine_handle<> h) {
            hook.h = h;
            if (auto res = impl->server.post_accept_task(nullptr, handler); !res) {
                hook.err = res.unwrap_err();
                hook.h.resume();
            }
        }
    };

    if (hook.err) {
        delete handler;
        throw std::runtime_error(hook.err.msg);
    }

    auto stream_impl 
        = new TcpStream::Impl{executor, &impl->server, hook, handler};
    co_return TcpStream{stream_impl};
}

// TcpClient

struct TcpClient::Impl {
    plat::IocpClient iocp_client;
};


CLASS_PIMPL_IMPLEMENT(TcpClient)

Coro<TcpClient> TcpClient::create() {
    if (auto res = plat::IocpClient::create(); res) {
        auto impl = new Impl{res.unwrap()};

        auto exe = co_await this_coro::executor;
        exe.waiting([impl] {
            int status = impl->iocp_client.wait_completion_task();
            if (status == ERROR_ABANDONED_WAIT_0 
                || status == ERROR_INVALID_HANDLE
            ) {
                return true;
            }

            return false;
        });

        co_return {impl};
    } else {
        throw std::runtime_error(res.unwrap_err().msg);
    }
}

Coro<TcpStream> TcpClient::connect(const char* host1, short port1, const char *host2, short port2) {
    auto exe = co_await this_coro::executor;
    RWHook hook{.exe = exe};

    auto handler = new plat::CompletionHandler{
        .hook = (void*)&hook,
        .cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (RWHook*)hook;
            rwhook->err = err;
            rwhook->sock = sock;
            rwhook->exe.post([h = rwhook->h] { h.resume(); });
        }
    };

    co_await Coro<void> {
        [&hook, impl = impl, handler, host1, port1, host2, port2](std::coroutine_handle<> h) {
            hook.h = h;
            if (auto res = impl->iocp_client.post_connect_task(host1, port1, host2, port2, handler); !res) {
                hook.err = res.unwrap_err();
                return h.resume();
            }
        }
    };

    if (hook.err) {
        delete handler;
        throw std::runtime_error(hook.err.msg);
    }

    auto stream_impl
         = new TcpStream::Impl{exe, &impl->iocp_client, hook, handler};
    co_return {stream_impl};
}

}