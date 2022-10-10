#include "magio/net/Tcp.h"

#include <memory>
#include "magio/dev/Log.h"
#include "magio/coro/Coro.h"
#include "magio/plat/loop.h"
#include "magio/plat/socket.h"
#include "magio/plat/errors.h"
#include "magio/plat/runtime.h"
#include "magio/net/IOService.h"
#include "magio/utils/ScopeGuard.h"

namespace magio {

struct EventHook {
    plat::socket_type fd;
    coroutine_handle<> h;
    std::error_code ec;
};

void resume_from_hook(
    std::error_code ec, 
    void* ptr, 
    plat::socket_type fd)
{
    auto phook = (EventHook*)ptr;
    phook->fd = fd;
    phook->ec = ec;
    phook->h.resume();
}

// TcpStream

struct TcpStream::Impl {
    plat::socket_type       fd;
    plat::IOLoop*           loop;

    SocketAddress           local;
    SocketAddress           remote;

    ~Impl() {
        plat::close_socket(fd);
    }
};

CLASS_PIMPL_IMPLEMENT(TcpStream)

Coro<TcpStream> TcpStream::connect(const char* host, uint_least16_t port) {
    auto exe = co_await this_coro::executor;
    if (auto ec = exe.get_service().open()) {
        throw std::system_error(ec);
    }

    auto loop = exe.get_service().get_loop();

    auto fd = plat::make_socket(plat::Protocol::TCP);
    if (plat::MAGIO_INVALID_SOCKET == fd) {
        MAGIO_THROW_SYSTEM_ERROR;
    }
    
    auto fd_guard = ScopeGuard{
        &fd, 
        [](plat::socket_type *p) {
            plat::close_socket(*p);
        }
    };

    
#ifdef __linux__
    if (auto ec = plat::set_nonblock(fd)) {
        throw std::system_error(ec);
    }

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
                impl->loop_ptr->async_connect(&evdata);
            }
        };
    }

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }
    
    auto io = plat::GlobalIO::get();
    io->fd = fd;

    fd_guard.release();
    co_return {new TcpStream::Impl{impl->loop_ptr, io}};
#endif
#ifdef _WIN32
    // local addr
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = 0; // auto

    if (-1 == ::inet_pton(AF_INET, host, &local_addr.sin_addr)) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    if (-1 == ::bind(fd, (sockaddr*)&local_addr, sizeof(local_addr))) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    if (auto ec = loop->relate(fd)) {
        throw std::system_error(ec);
    }

    auto hook = EventHook{};

    plat::IOData io;
    io.fd = fd;
    io.ptr = &hook;
    io.cb = resume_from_hook;

    co_await Coro<> {
        [host, port, pio = &io, loop, &hook](coroutine_handle<> h) {
            hook.h = h;
            loop->async_connect(pio, host, port);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return {
        new TcpStream::Impl{
            *fd_guard.release(),
            loop, 
            plat::local_address(fd),
            SocketAddress{host, port}
        }};
#endif
}

Coro<size_t> TcpStream::read(char* buf, size_t len) {
    auto hook = EventHook{};
#ifdef __linux__
    auto evdata = plat::EventData{
        .fd = impl->iodata->fd,
        .io = impl->iodata,
        .data = &hook,
        .cb = resume_from_hook
    };

    co_await Coro<void>{
        [this, &hook, &evdata](coroutine_handle<> h) {
            hook.h = h;
            impl->loop_ptr->async_receive(&evdata);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    auto [buf, len] = impl->iodata->input_buffer;
#endif
#ifdef _WIN32
    plat::IOData io;
    io.fd = impl->fd;
    io.wsa_buf.buf = buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook;

    co_await Coro<>{
        [this, &hook, pio = &io](coroutine_handle<> h) {
            hook.h = h;
            impl->loop->async_receive(pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }
#endif

    co_return io.wsa_buf.len;
}

Coro<size_t> TcpStream::write(const char* buf, size_t len) {
    auto hook = EventHook{};
#ifdef __linux__
    auto evdata = plat::EventData{
        .fd = impl->iodata->fd,
        .io = impl->iodata,
        .data = &hook,
        .cb = resume_from_hook
    };

    co_await Coro<void>{
        [this, &hook, &evdata](coroutine_handle<> h) {
            hook.h = h;
            impl->loop_ptr->async_send(&evdata);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return impl->iodata->output_buffer.len;
#endif
#ifdef _WIN32
    plat::IOData io;
    io.fd = impl->fd;
    io.wsa_buf.buf = (char*)buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook;

    co_await Coro<>{
        [this, &hook, pio = &io](coroutine_handle<> h) {
            hook.h = h;
            impl->loop->async_send(pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return io.wsa_buf.len;
#endif
}

SocketAddress TcpStream::local_address() {
    return impl->local;
}

SocketAddress TcpStream::remote_address() {
    return impl->remote;
}


// TcpServer

struct TcpServer::Impl {
    plat::socket_type   fd;
    plat::IOLoop*       loop;

    ~Impl() {
        plat::close_socket(fd);
    }
};

CLASS_PIMPL_IMPLEMENT(TcpServer)

Coro<TcpServer> TcpServer::bind(const char* host, uint_least16_t port) {
    auto exe = co_await this_coro::executor;
    if (auto ec = exe.get_service().open()) {
        throw std::system_error(ec);
    }

    auto loop = exe.get_service().get_loop();

    auto fd = plat::make_socket(plat::Protocol::TCP);
    if (plat::MAGIO_INVALID_SOCKET == fd) {
        MAGIO_THROW_SYSTEM_ERROR;
    }
    
    auto fd_guard = ScopeGuard {
        &fd, 
        [](plat::socket_type* fd) {
            plat::close_socket(*fd);
        }
    };

#ifdef __linux__
    // 非阻塞
    if (auto ec = plat::set_nonblock(fd)) {
        throw std::system_error(ec);
    }
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (-1 == ::inet_pton(AF_INET, host, &addr.sin_addr)) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    if (-1 == ::bind(fd, (sockaddr*)&addr, sizeof(addr))) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    if (-1 == ::listen(fd, SOMAXCONN)) {
        MAGIO_THROW_SYSTEM_ERROR;
    }
#ifdef __linux__
    // 端口重用和地址重用
    int opt_val = 1;
    if (-1 == ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val))) {
        THROW_SYSTEM_ERROR;
    }
    
    if (-1 == ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val))) {
        THROW_SYSTEM_ERROR;
    }
#endif
#ifdef _WIN32
    // 关联iocp
    if (auto ec = loop->relate(fd)) {
        throw std::system_error(ec);
    }
#endif

    co_return {new Impl{*fd_guard.release(), loop}};
}

Coro<TcpStream> TcpServer::accept() {
    auto hook = EventHook{};

#ifdef __linux__
    auto evdata = plat::EventData{
        .data = &hook,
        .cb = resume_from_hook
    };
#endif
#ifdef _WIN32
    char buf[128];
    plat::IOData io;

    io.fd = plat::make_socket(plat::Protocol::TCP);
    if (plat::MAGIO_INVALID_SOCKET == io.fd) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    auto fd_guard = ScopeGuard{
        &io.fd,
        [](plat::socket_type* p) {
            plat::close_socket(*p);
        }
    };

    io.wsa_buf.buf = buf;
    io.wsa_buf.len = sizeof(buf);
    io.ptr = &hook;
    io.cb = resume_from_hook;
#endif

    co_await Coro<void>{
        [this, pio = &io, &hook](coroutine_handle<> h) {
            hook.h = h;
            impl->loop->async_accept(impl->fd, pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

#ifdef __linux__
    auto fd_guard = ScopeGuard{&hook.fd, [](plat::socket_type* p) {
        plat::close_socket(*p);
    }};

    if (auto ec = plat::set_nonblock(hook.fd)) {
        throw std::system_error(ec);
    }

    if (auto ec = loop_ptr->add(hook.fd)) {
        throw std::system_error(ec);
    }

    auto iodata = plat::GlobalIO::get();
    iodata->fd = hook.fd;

    fd_guard.release();
    co_return {new TcpStream::Impl{loop_ptr, iodata}};
#endif
#ifdef _WIN32
    if (auto ec = impl->loop->relate(io.fd)) {
        throw std::system_error(ec);
    }

    ZeroMemory(buf, sizeof(buf));
    ::inet_ntop(io.local.sin_family, &io.local.sin_addr, buf, 40);
    SocketAddress local{buf, ::ntohs(io.local.sin_port), io.local};

    ZeroMemory(buf, sizeof(buf));
    ::inet_ntop(io.remote.sin_family, &io.remote.sin_addr, buf, 40);
    SocketAddress remote{buf, ::ntohs(io.remote.sin_port), io.remote};

    co_return {new TcpStream::Impl{
        *fd_guard.release(),
        impl->loop,
        std::move(local),
        std::move(remote)
    }};
#endif
}

}