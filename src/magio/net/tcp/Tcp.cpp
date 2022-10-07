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

namespace magio {

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

Coro<std::string_view> TcpStream::read() {
    auto hook = EventHook{};

    auto evdata = plat::EventData{
        .fd = impl->iodata->fd,
        .op = plat::IOOP::Receive,
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
    co_return std::string_view(buf, len);
}

Coro<size_t> TcpStream::write(const char* buf, size_t len) {
    size_t cplen = global_config.buffer_size > len ? len : global_config.buffer_size;
    std::memcpy(impl->iodata->output_buffer.buf, buf, cplen);
    impl->iodata->output_buffer.len = cplen;

    auto hook = EventHook{};

    auto evdata = plat::EventData{
        .fd = impl->iodata->fd,
        .op = plat::IOOP::Send,
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
        for (auto& loop : loops) {
            loop->close();
        }
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
    std::shared_ptr<plat::IOLoop> loop_ptr;

    ~Impl() {
        loop_ptr->close();
    }
};

CLASS_PIMPL_IMPLEMENT(TcpClient)

Coro<TcpClient> TcpClient::create() {
    auto expect_loop = plat::IOLoop::create(global_config.max_sockets, -2);
    if (!expect_loop) {
        throw std::system_error(expect_loop.unwrap_err());
    }
    auto loop_ptr = std::make_shared<plat::IOLoop>(expect_loop.unwrap());

    auto exe = co_await this_coro::executor;
    exe.waiting([loop_ptr] {
        if (auto ec = loop_ptr->poll(0)) {
            std::printf("main loop: %s\n", ec.message().c_str());
            return true;
        }
        return false;
    });

    co_return {new Impl{loop_ptr}};
}

Coro<TcpStream> TcpClient::connect(const char* host, uint_least16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (-1 == fd) {
        THROW_SYSTEM_ERROR;
    }

    auto fd_guard = ScopeGuard{&fd, [](int *p) {
        ::close(*p);
    }};

    ::sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    if (-1 == ::inet_pton(AF_INET, host, &addr.sin_addr)) {
        THROW_SYSTEM_ERROR;
    }

    int connres = ::connect(fd, (sockaddr*)&addr, sizeof(addr));
    if (-1 == connres && errno != EINPROGRESS) {
        THROW_SYSTEM_ERROR;
    }

    auto hook = EventHook{};
    auto evdata = plat::EventData{
        .fd = fd,
        .op = plat::IOOP::Connect,
        .data = &hook,
        .cb = resume_from_hook
    };
    
    if (0 != connres) {
        co_await Coro<void>{
            [&hook, &evdata, this, fd](coroutine_handle<> h) {
                hook.h = h;
                impl->loop_ptr->async_connect(fd, &evdata);
            }
        };
    }

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }
    
    auto io = plat::global_io().get();
    io->fd = fd;

    fd_guard.release();
    co_return {new TcpStream::Impl{impl->loop_ptr, io}};
}

#endif

}