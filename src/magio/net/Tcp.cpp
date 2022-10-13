#include "magio/net/Tcp.h"

#include <memory>
#include "magio/dev/Log.h"
#include "magio/coro/Coro.h"
#include "magio/plat/io_service.h"

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

Coro<TcpStream> TcpStream::connect(const char* host, uint_least16_t port) {
    auto exe = co_await this_coro::executor;

    auto socket = Socket::create(exe, Protocol::TCP).expect();

#ifdef _WIN32
    auto local_address = make_address("127.0.0.1", 0).expect();
    socket.bind(local_address).expect();

    if (auto ec = exe.get_service().relate(socket.handle())) {
        throw std::system_error(ec);
    }
#endif
    auto remote_address = make_address(host, port).expect();

    auto hook = EventHook{};

    plat::IOData io;
    io.fd = socket.handle();
    io.ptr = &hook;
    io.cb = resume_from_hook;

    co_await Coro<> {
        [&hook, &remote_address, exe, pio = &io](coroutine_handle<> h) mutable {
            hook.h = h;
            exe.get_service().async_connect(pio, remote_address._sockaddr);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return TcpStream{
        std::move(socket), 
        plat::local_address(socket.handle()),
        remote_address};
}

Coro<size_t> TcpStream::read(char* buf, size_t len) {
    auto hook = EventHook{};

    plat::IOData io;
    io.fd = socket_.handle();
    io.wsa_buf.buf = buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook;
    
    co_await Coro<>{
        [this, &hook, pio = &io](coroutine_handle<> h) {
            hook.h = h;
            socket_.get_executor().get_service().async_receive(pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return io.wsa_buf.len;
}

Coro<size_t> TcpStream::write(const char* buf, size_t len) {
    auto hook = EventHook{};

    plat::IOData io;
    io.fd = socket_.handle();
    io.wsa_buf.buf = (char*)buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook;

    co_await Coro<>{
        [this, &hook, pio = &io](coroutine_handle<> h) {
            hook.h = h;
            socket_.get_executor().get_service().async_send(pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return io.wsa_buf.len;
}

SocketAddress TcpStream::local_address() {
    return local_;
}

SocketAddress TcpStream::remote_address() {
    return remote_;
}

AnyExecutor TcpStream::get_executor() {
    return socket_.get_executor();
}

// TcpServer

Coro<TcpServer> TcpServer::bind(const char* host, uint_least16_t port) {
    auto exe = co_await this_coro::executor;

    auto socket = Socket::create(exe, Protocol::TCP).expect();
    auto address = make_address(host, port).expect();
    socket.bind(address).expect();
    socket.listen().expect();

#ifdef __linux__
    // 端口重用和地址重用
    int opt_val = 1;
    if (-1 == ::setsockopt(socket.handle(), SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val))) {
        MAGIO_THROW_SYSTEM_ERROR;
    }
    
    if (-1 == ::setsockopt(socket.handle(), SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val))) {
        MAGIO_THROW_SYSTEM_ERROR;
    }
#endif
#ifdef _WIN32
    // 关联iocp
    if (auto ec = exe.get_service().relate(socket.handle())) {
        throw std::system_error(ec);
    }
#endif

    co_return {std::move(socket)};
}

Coro<TcpStream> TcpServer::accept() {
    auto hook = EventHook{};

    plat::IOData io;
#ifdef _WIN32
    auto socket = Socket::create(listener.get_executor(), Protocol::TCP).expect();

    char buf[128];

    io.fd = socket.handle();
    io.wsa_buf.buf = buf;
    io.wsa_buf.len = sizeof(buf);
#endif
    io.ptr = &hook;
    io.cb = resume_from_hook;

    co_await Coro<>{
        [&hook, this, pio = &io](coroutine_handle<> h) mutable {
            hook.h = h;
            listener.get_executor().get_service().async_accept(listener.handle(), pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

#ifdef __linux__
    co_return TcpStream{
        Socket(listener.get_executor(), io.fd), 
        plat::local_address(io.fd), 
        {::inet_ntoa(io.remote.sin_addr), ::ntohs(io.remote.sin_port)}};
#endif
#ifdef _WIN32
    if (auto ec = listener.get_executor().get_service().relate(io.fd)) {
        throw std::system_error(ec);
    }

    ZeroMemory(buf, sizeof(buf));
    ::inet_ntop(io.local.sin_family, &io.local.sin_addr, buf, 40);
    SocketAddress local{buf, ::ntohs(io.local.sin_port), io.local};

    ZeroMemory(buf, sizeof(buf));
    ::inet_ntop(io.remote.sin_family, &io.remote.sin_addr, buf, 40);
    SocketAddress remote{buf, ::ntohs(io.remote.sin_port), io.remote};

    co_return {
        std::move(socket),
        std::move(local),
        std::move(remote)
    };
#endif
}

AnyExecutor TcpServer::get_executor() {
    return listener.get_executor();
}

}