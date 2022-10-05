#include "magio/net/tcp/Tcp.h"

#include <memory>
#include "magio/Configs.h"
#include "magio/dev/Log.h"
#include "magio/coro/Coro.h"
#include "magio/plat/io.h"
#include "magio/plat/loop.h"
#include "magio/plat/socket.h"
#include "magio/plat/errors.h"
#include "magio/plat/runtime.h"
#include "magio/utils/ScopeGuard.h"

#ifdef _WIN32
#include "magio/plat/iocp/iocp.h"
#include "magio/plat/iocp/socket.h"
#elif __linux__

#endif

namespace magio {

#ifdef _WIN32

struct RWHook {
    Error err;
    plat::SocketHelper sock{nullptr};
    coroutine_handle<> h;
    AnyExecutor exe;
};

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
            rwhook->exe.post([h = rwhook->h]() mutable { h.resume(); });
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

Coro<std::tuple<char*, size_t>> TcpStream::read() {
    impl->hook_.exe = co_await this_coro::executor;
    impl->hook_.err = Error();

    co_await Coro<void>{
        [impl = impl](coroutine_handle<> h) {
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

Coro<size_t> TcpStream::write(const char *buf, size_t len) {
    impl->hook_.exe = co_await this_coro::executor;
    impl->hook_.err = Error();

    size_t cp_len = impl->hook_.sock.send_io().capacity() > len ? len : impl->hook_.sock.send_io().capacity();
    std::memcpy(impl->hook_.sock.send_io().buf(), buf, cp_len);
    impl->hook_.sock.send_io().set_len(cp_len);

    co_await Coro<void>{
        [impl = impl](coroutine_handle<> h) {
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

Coro<TcpServer> TcpServer::bind(const char* host, uint_least16_t port) {
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
            rwhook->exe.post([rwhook]() mutable { rwhook->h.resume(); });
        }
    });

    co_await Coro<void>{
        [&hook, ch = handler.get(), impl = impl](coroutine_handle<> h) {
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

Coro<TcpStream> TcpClient::connect(const char* host1, uint_least16_t port1, const char *host2, uint_least16_t port2) {
    auto exe = co_await this_coro::executor;
    RWHook hook{.exe = exe};

    auto handler = ScopeGuard(new plat::CompletionHandler{
        .hook = (void*)&hook,
        .cb = [](void* hook, Error err, plat::SocketHelper sock) {
            auto rwhook = (RWHook*)hook;
            rwhook->err = err;
            rwhook->sock = sock;
            rwhook->exe.post([h = rwhook->h]() mutable { h.resume(); });
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

#endif

#ifdef __linux__

struct EventHook {
    int fd;
    coroutine_handle<> h;
    std::error_code ec;
};

void resume_from_hook(std::error_code ec, void* ptr, int fd) {
    auto phook = (EventHook*)ptr;
    phook->fd = fd;
    phook->ec = ec;
    phook->h.resume();
}

// TcpStream

struct TcpStream::Impl {
    std::shared_ptr<plat::IOLoop> loop_ptr;
    plat::IOData* iodata;

    ~Impl() {
        ::close(iodata->fd);
        plat::global_io().put(iodata);
    }
};

CLASS_PIMPL_IMPLEMENT(TcpStream)

Coro<std::tuple<char*, size_t>> TcpStream::read() {
    auto hook = EventHook{};

    auto evdata = plat::EventData{
        .fd = impl->iodata->fd,
        .io = impl->iodata,
        .data = &hook,
        .cb = resume_from_hook
    };

    co_await Coro<void>{
        [this, &hook, &evdata](coroutine_handle<> h) {
            hook.h = h;
            impl->loop_ptr->async_receive(impl->iodata->fd, &evdata);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    auto [buf, len] = impl->iodata->input_buffer;
    co_return std::make_tuple(buf, len);
}

Coro<size_t> TcpStream::write(const char* buf, size_t len) {
    size_t cplen = global_config.buffer_size > len ? len : global_config.buffer_size;
    std::memcpy(impl->iodata->output_buffer.buf, buf, cplen);
    impl->iodata->output_buffer.len = cplen;

    auto hook = EventHook{};

    auto evdata = plat::EventData{
        .fd = impl->iodata->fd,
        .io = impl->iodata,
        .data = &hook,
        .cb = resume_from_hook
    };

    co_await Coro<void>{
        [this, &hook, &evdata](coroutine_handle<> h) {
            hook.h = h;
            impl->loop_ptr->async_send(impl->iodata->fd, &evdata);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return impl->iodata->output_buffer.len;
}

Address TcpStream::local_address() {
    return plat::local_address(impl->iodata->fd);
}

Address TcpStream::remote_address() {
    return plat::remote_address(impl->iodata->fd);
}

// TcpServer

struct TcpServer::Impl {
    std::vector<std::shared_ptr<plat::IOLoop>> loops;
    size_t index = 0;

    ~Impl() {
        
    }
};

CLASS_PIMPL_IMPLEMENT(TcpServer)

Coro<TcpServer> TcpServer::bind(const char* host, uint_least16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (-1 == fd) {
        THROW_SYSTEM_ERROR;
    }

    auto fd_guard = ScopeGuard{&fd, [](int* fd) {
        ::close(*fd);
    }};

    ::sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (-1 == ::inet_pton(AF_INET, host, &addr.sin_addr)) {
        THROW_SYSTEM_ERROR;
    }

    if (-1 == ::bind(fd, (sockaddr*)&addr, sizeof(addr))) {
        THROW_SYSTEM_ERROR;
    }

    if (-1 == ::listen(fd, SOMAXCONN)) {
        THROW_SYSTEM_ERROR;
    }

    // 允许多个socket绑定同一个端口
    int opt_val = 1;
    if (-1 == setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val))) {
        THROW_SYSTEM_ERROR;
    }

    std::vector<std::shared_ptr<plat::IOLoop>> loops;
    for (size_t i = 0; i < global_config.worker_threads; ++i) {
        if (auto res = plat::IOLoop::create(global_config.max_sockets, i == 0 ? fd : -1)) {
            loops.emplace_back(std::make_shared<plat::IOLoop>(res.unwrap()));
        } else {
            throw std::system_error(res.unwrap_err());
        }
    }

    // main loop
    auto exe = co_await this_coro::executor;
    exe.waiting([loop_ptr = loops[0]] {
        auto ec = loop_ptr->poll(0);
        if (ec) {
            std::printf("main loop: %s\n", ec.message().c_str());
            return true;
        }
        return false;
    });
    
    // vice loop
    for (size_t i = 1; i < global_config.worker_threads; ++i) {
        plat::Runtime::ins().post(
            [loop_ptr = loops[i]] {
                for (; ;) {
                    if (auto ec = loop_ptr->poll(-1)) {
                        // log
                        std::printf("vice loop: %s\n", ec.message().c_str());
                        return;
                    }
                }
            });
    }

    fd_guard.release();
    co_return {new Impl{std::move(loops)}};
}

Coro<TcpStream> TcpServer::accept() {
    auto hook = EventHook{};

    auto evdata = plat::EventData{
        .data = &hook,
        .cb = resume_from_hook
    };

    // 负载均衡
    auto cur = impl->index;
    impl->index = (cur + 1) % impl->loops.size();
    auto loop_ptr = impl->loops[cur];

    co_await Coro<void>{
        [this, &hook, &evdata](coroutine_handle<> h) {
            hook.h = h;
            impl->loops[0]->async_accept(&evdata);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    auto fd_guard = ScopeGuard{&hook.fd, [](int* p) {
        ::close(*p);
    }};

    // 非阻塞化
    if (auto ec = plat::set_nonblock(hook.fd)) {
        throw std::system_error(ec);
    }

    if (auto ec = loop_ptr->add(hook.fd)) {
        throw std::system_error(ec);
    }

    auto iodata = plat::global_io().get();
    iodata->fd = hook.fd;

    fd_guard.release();
    co_return {new TcpStream::Impl{loop_ptr, iodata}};
}

// TcpClient

struct TcpClient::Impl {
    inline static std::shared_ptr<plat::IOLoop> loop_ptr;
};

CLASS_PIMPL_IMPLEMENT(TcpClient)

Coro<TcpStream> TcpClient::connect(const char* host, uint_least16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (-1 == fd) {
        THROW_SYSTEM_ERROR;
    }

    ::sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    if (-1 == ::inet_pton(AF_INET, host, &addr.sin_addr)) {
        THROW_SYSTEM_ERROR;
    }

    if (-1 == ::connect(fd, (sockaddr*)&addr, sizeof(addr))) {
        THROW_SYSTEM_ERROR;
    }

    auto expect_loop = plat::IOLoop::create(global_config.max_sockets, fd);
    if (!expect_loop) {
        throw std::system_error(expect_loop.unwrap_err());
    }
    impl->loop_ptr = std::make_shared<plat::IOLoop>(expect_loop.unwrap());


    auto io = plat::global_io().get();
    io->fd = fd;

    co_return {new TcpStream::Impl{impl->loop_ptr, io}};
}


#endif

}