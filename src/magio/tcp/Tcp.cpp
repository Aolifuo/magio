#include "magio/tcp/Tcp.h"

#include <iostream>
#include "magio/Configs.h"
#include "magio/coro/Coro.h"
#include "magio/plat/iocp.h"
#include "magio/plat/socket.h"
#include "magio/plat/errors.h"
#include "magio/utils/ScopeGuard.h"

namespace magio {

struct RWHook {
    Error err;
    plat::SocketHelper sock{nullptr};
    std::coroutine_handle<> h;
    AnyExecutor exe;
};

static thread_local AnyExecutor ThreadGlobalExecutor;

// TcpStream

struct TcpStream::Impl {
    plat::IocpInterface* iocp_;
    RWHook hook_;
    plat::CompletionHandler* handler_;
    
    Impl(plat::IocpInterface* iocp, RWHook hook, plat::CompletionHandler* handler)
        : iocp_(iocp), hook_(hook), handler_(handler)
    {
        handler_->hook = (void*)&hook_;
        handler_->cb = [](void* hook, Error err, plat::SocketHelper sock) {
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

Address TcpStream::local_address() {
    return impl->hook_.sock.local_addr();
}

Address TcpStream::remote_address() {
    return impl->hook_.sock.remote_addr();
}

Coro<std::tuple<char*, size_t>> TcpStream::vread() {
    impl->hook_.exe = co_await this_coro::executor;
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

    auto io = impl->hook_.sock.recv_io();

    co_return std::make_tuple(io.buf(), io.len());
}

Coro<size_t> TcpStream::read(char *buf, size_t len) {
    impl->hook_.exe = co_await this_coro::executor;
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
    impl->hook_.exe = co_await this_coro::executor;
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
        executor.waiting([impl]() -> bool {
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

    auto handler = ScopeGuard(new plat::CompletionHandler{
        .hook = (void*)&hook, 
        .cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (RWHook*)hook;
            rwhook->err = err;
            rwhook->sock = sock;
            rwhook->exe.post([rwhook] { rwhook->h.resume(); });
        }
    });

    co_await Coro<void>{
        [&hook, ch = handler.get(), impl = impl](std::coroutine_handle<> h) {
            hook.h = h;
            if (auto res = impl->server.post_accept_task(nullptr, ch); !res) {
                hook.err = res.unwrap_err();
                hook.h.resume();
            }
        }
    };

    if (hook.err) {
        throw std::runtime_error(hook.err.msg);
    }

    auto stream_impl 
        = new TcpStream::Impl{&impl->server, hook, handler.release()};
    co_return TcpStream{stream_impl};
}

// TcpClient

struct TcpClient::Impl {
    AnyExecutor executor;
    plat::IocpClient iocp_client;
};


CLASS_PIMPL_IMPLEMENT(TcpClient)

Coro<TcpClient> TcpClient::create() {
    if (auto res = plat::IocpClient::create(); res) {
        auto impl = new Impl{co_await this_coro::executor, res.unwrap()};

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

    auto handler = ScopeGuard(new plat::CompletionHandler{
        .hook = (void*)&hook,
        .cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (RWHook*)hook;
            rwhook->err = err;
            rwhook->sock = sock;
            rwhook->exe.post([h = rwhook->h] { h.resume(); });
        }
    });

    co_await Coro<void> {
        [&hook, impl = impl, ch = handler.get(), host1, port1, host2, port2](std::coroutine_handle<> h) {
            hook.h = h;
            if (auto res = impl->iocp_client.post_connect_task(host1, port1, host2, port2, ch); !res) {
                hook.err = res.unwrap_err();
                return h.resume();
            }
        }
    };

    if (hook.err) {
        throw std::runtime_error(hook.err.msg);
    }

    hook.sock.local_addr() = {(unsigned)port1, host1};
    hook.sock.remote_addr() = {(unsigned)port2, host2};

    auto stream_impl
         = new TcpStream::Impl{&impl->iocp_client, hook, handler.release()};
    co_return {stream_impl};
}


}