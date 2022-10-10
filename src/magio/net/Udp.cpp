#include "magio/net/Udp.h"

#include "magio/dev/Log.h"
#include "magio/coro/Coro.h"
#include "magio/plat/loop.h"
#include "magio/plat/runtime.h"
#include "magio/utils/ScopeGuard.h"

namespace magio {

struct UdpHook {
    plat::socket_type fd;
    coroutine_handle<> h;
    std::error_code ec;
};

void resume_from_hook2(
    std::error_code ec, 
    void* ptr, 
    plat::socket_type fd)
{
    auto phook = (UdpHook*)ptr;
    phook->fd = fd;
    phook->ec = ec;
    phook->h.resume();
}

// UdpSocket

struct UdpSocket::Impl {
    plat::socket_type   fd;
    plat::IOLoop*       loop;

    ~Impl() {
        plat::close_socket(fd);
    }
};

CLASS_PIMPL_IMPLEMENT(UdpSocket)

Coro<UdpSocket> UdpSocket::bind(const char* host, uint_least16_t port) {
    auto exe = co_await this_coro::executor;
    if (auto ec = exe.get_service().open()) {
        throw std::system_error(ec);
    }

    auto loop = exe.get_service().get_loop();
    
    auto fd = plat::make_socket(plat::Protocol::UDP);
    if (plat::MAGIO_INVALID_SOCKET == fd) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    auto fd_guard = ScopeGuard{
        &fd, 
        [](plat::socket_type* fd) {
            plat::close_socket(*fd);
        }
    };

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (-1 == ::inet_pton(AF_INET, host, &addr.sin_addr)) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    if (-1 == ::bind(fd, (sockaddr*)&addr, sizeof(addr))) {
        MAGIO_THROW_SYSTEM_ERROR;
    }

    if (auto ec = loop->relate(fd)) {
        throw std::system_error(ec);
    }

    co_return {new Impl{*fd_guard.release(), loop}};
}

Coro<std::pair<size_t, SocketAddress>> UdpSocket::read_from(char* buf, size_t len) {
    auto hook = UdpHook{};

    plat::IOData io;
    io.fd = impl->fd;
    io.wsa_buf.buf = buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook2;

    co_await Coro<> {
        [&hook, this, pio = &io](coroutine_handle<> h) {
            hook.h = h;
            impl->loop->async_receive_from(pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    char addr_buf[40]{};
    ::inet_ntop(
        io.remote.sin_family,
        &io.remote.sin_addr,  
        addr_buf, 40);

    co_return {io.wsa_buf.len, {addr_buf, ::ntohs(io.remote.sin_port), io.remote}};
}

Coro<size_t> UdpSocket::write_to(const char* buf, size_t len, const SocketAddress& address) {
    auto hook = UdpHook{};

    plat::IOData io;

    if (0 == address._sockaddr.sin_port) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.host);

        if (-1 == ::inet_pton(AF_INET, address.ip.c_str(), &addr.sin_addr)) {
            MAGIO_THROW_SYSTEM_ERROR;
        }
        
        io.remote = addr;   
    } else {
        io.remote = address._sockaddr;
    }

    io.fd = impl->fd;
    io.wsa_buf.buf = (char*)buf;
    io.wsa_buf.len = len;
    io.ptr = &hook;
    io.cb = resume_from_hook2;

    co_await Coro<> {
        [&hook, this, pio = &io](coroutine_handle<> h) {
            hook.h = h;
            impl->loop->async_send_to(pio);
        }
    };

    if (hook.ec) {
        throw std::system_error(hook.ec);
    }

    co_return io.wsa_buf.len;
}

}